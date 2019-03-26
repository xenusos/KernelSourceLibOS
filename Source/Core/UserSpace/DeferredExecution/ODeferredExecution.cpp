/*
    Purpose: A Windows-APC style thread preemption within the Linux kernel [depends on latest xenus linux kernel patch]
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#include <libos.hpp>
#include <Core/CPU/OWorkQueue.hpp>
#include "ODeferredExecution.hpp"

#include "ODECriticalSection.hpp"
#include "ODEReturn.hpp"
#include "ODEProcess.hpp"
#include "ODEThread.hpp"
#include "../../Processes/OProcesses.hpp"

static mutex_k work_watcher_mutex;

static error_t APC_AddPendingWork(task_k tsk, ODEWorkHandler * impl);

ODEWorkJobImpl::ODEWorkJobImpl(task_k task, OPtr<OWorkQueue> workqueue)
{
    LogFunction;
    _worker           = nullptr;
    _state.execd      = false;
    _state.dispatched = false;
    _work             = { 0 };
    _task             = task;
    _callback.func    = nullptr;
    _callback.data    = nullptr;
    _workqueue        = workqueue;

    ProcessesTaskIncrementCounter(task);
}

error_t ODEWorkJobImpl::SetWork(ODEWork & work)
{
    LogFunction;
    CHK_DEAD;
    _work = work;
    return kStatusOkay;
}

error_t ODEWorkJobImpl::Schedule()
{
    LogFunction;
    CHK_DEAD;
    error_t err;
    
    if (_worker)
        return kErrorInternalError;
    
    _worker = new ODEWorkHandler(_task, this);
    
    if (!_worker)
        return kErrorOutOfMemory;
    
    _worker->SetWork(_work);
    _state.dispatched = true;
    
    return _worker->Schedule();
}

error_t ODEWorkJobImpl::HasDispatched(bool & dispatched) 
{
    LogFunction;
    CHK_DEAD;
    dispatched = _state.dispatched;
    return kStatusOkay;
}

error_t ODEWorkJobImpl::HasExecuted(bool & executed)
{
    LogFunction;
    CHK_DEAD;
    executed = _state.execd;
    return kStatusOkay;
}

error_t ODEWorkJobImpl::WaitExecute(uint32_t ms)               
{
    LogFunction;
    CHK_DEAD;
    error_t err;
    if (STRICTLY_OKAY(err = _workqueue->WaitAndAddOwner(ms)))
        _workqueue->ReleaseOwner();
    return err;
}

error_t ODEWorkJobImpl::AwaitExecute(ODECompleteCallback_f cb, void * context)
{
    LogFunction;
    CHK_DEAD;
    if (_callback.func)
        return kErrorInternalError;
    _callback.func = cb;
    _callback.data = context;
    return kStatusOkay;
}

error_t ODEWorkJobImpl::GetResponse(size_t & ret)
{
    LogFunction;
    CHK_DEAD;
    ret = _state.response;
    return kStatusOkay;
}

void ODEWorkJobImpl::InvalidateImp()
{
    LogFunction;
    mutex_lock(work_watcher_mutex);
    if (_worker)
        _worker->Fuckoff();
    mutex_unlock(work_watcher_mutex);

    _workqueue->Destory();

    if (_task)
        ProcessesTaskDecrementCounter(_task);
}

void ODEWorkJobImpl::Fuckoff()
{
    LogFunction;
    _worker = nullptr;
}

void ODEWorkJobImpl::Trigger(size_t response)
{
    LogFunction;
    _state.response = response;
    _state.execd    = true;
}

void ODEWorkJobImpl::GetCallback(ODECompleteCallback_f & callback, void * & context)
{
    LogFunction;
    callback = _callback.func;
    context  = _callback.data;
}

ODEWorkHandler::ODEWorkHandler(task_k tsk, ODEWorkJobImpl * worker)
{
    LogFunction;
     _tsk             = tsk;
     _parant          = worker;
     ProcessesTaskIncrementCounter(_tsk);
}

ODEWorkHandler::~ODEWorkHandler()
{
    LogFunction;
    if (_tsk)
        ProcessesTaskDecrementCounter(_tsk);
}

void ODEWorkHandler::ParseRegisters(pt_regs & regs)
{
    LogFunction;
    regs.rip = _work.address;

    if (_work.cc == kODESysV)
    {
        regs.rdi = _work.parameters.one;
        regs.rsi = _work.parameters.two;
        regs.rdx = _work.parameters.three;
        regs.rcx = _work.parameters.four;
    }
    else if (_work.cc == kODEWin64)
    {
        regs.rcx = _work.parameters.one;
        regs.rdx = _work.parameters.two;
        regs.r8  = _work.parameters.three;
        regs.r9  = _work.parameters.four;
    }
}

void ODEWorkHandler::Fuckoff()
{
    LogFunction;
    _parant = nullptr;
}

void ODEWorkHandler::Hit(size_t response)
{
    LogFunction;
    ODECompleteCallback_f callback = nullptr;
    void * context                 = nullptr;
    
    mutex_lock(work_watcher_mutex);
    if (_parant)
    {
        _parant->Trigger(response);
        _parant->GetCallback(callback, context);
        _parant->Fuckoff();
    }
    mutex_unlock(work_watcher_mutex);

    if (callback)
        callback(context);

    delete this;
}

void ODEWorkHandler::Die()
{
    LogFunction;
    mutex_lock(work_watcher_mutex);
    if (_parant)
    {
        _parant->Fuckoff();
    }
    mutex_unlock(work_watcher_mutex);
    delete this;
}

error_t ODEWorkHandler::SetWork(ODEWork & work)
{
    _work = work;
    return kStatusOkay;
}

error_t ODEWorkHandler::Schedule()
{
    LogFunction;
    error_t err;

    if (!_tsk)
        return kErrorInternalError;

    err = APC_AddPendingWork(_tsk, this);

    if (ERROR(err))
        return err;

    //ProcessesTaskDecrementCounter(_tsk);
    //_tsk = nullptr;
    return kStatusOkay;
}


static void APC_OnThreadExit(OPtr<OProcess> thread)
{
    LogFunction;
    void * handle;
    error_t err;

    EnterDECriticalSection();

    err = thread->GetOSHandle(&handle);
    ASSERT(NO_ERROR(err), "Error: 0x%zx", err);

    FreeDEProcess((task_k)handle);
    LeaveDECriticalSection();
}

void DeferredExecFinish(size_t ret)
{
    LogFunction;
    error_t err;
    ODEImplPIDThread * thread;

    EnterDECriticalSection();

    err = GetDEThread(thread, OSThread);
    if (ERROR(err))
    {
        LogPrint(kLogError, "Couldn't acquire DE thread instance... SYSCALL ABUSE? (error: 0x%zx)", err);
        LeaveDECriticalSection();
        return;
    }
    
    thread->NtfyJobFinished(ret);

    LeaveDECriticalSection();
}

static error_t APC_AddPendingWork(task_k tsk, ODEWorkHandler * impl)
{
    LogFunction;
    error_t err;
    ODEImplPIDThread * thread;

    EnterDECriticalSection();

    thread = nullptr;
    err = GetOrCreateDEThread(thread, tsk);
    if (ERROR(err))
    {
        LogPrint(kLogError, "Couldn't create DE thread instance (error: 0x%zx)", err);
        LeaveDECriticalSection();
        return err;
    }

    thread->AppendWork(impl);

    LeaveDECriticalSection();
    return kStatusOkay;
}


void InitDeferredCalls()
{
    LogFunction;
    ProcessesAddExitHook(APC_OnThreadExit);

    InitDEReturn();
    InitDECriticalSection();
    InitDEProcesses();

    work_watcher_mutex = mutex_create();
}

LIBLINUX_SYM error_t CreateWorkItem(OPtr<OProcessThread> target, const OOutlivableRef<ODEWorkJob> out)
{
    error_t err;
    task_k handle;
    OPtr<OWorkQueue> wq;

    if (!target.GetTypedObject())
        return kErrorIllegalBadArgument;

    if (ERROR(err = CreateWorkQueue(1, wq)))
        return err;

    if (ERROR(err = target->GetOSHandle((void **)&handle)))
        return err;

    if (!out.PassOwnership(new ODEWorkJobImpl(handle, wq)))
        return kErrorOutOfMemory;

    return kStatusOkay;
}
