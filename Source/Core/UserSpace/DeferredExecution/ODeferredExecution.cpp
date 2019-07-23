/*
    Purpose: A Windows-APC style thread preemption within the Linux kernel [depends on latest xenus linux kernel patch]
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#include <libos.hpp>
#include <Core/CPU/OWorkQueue.hpp>
#include <Core/Processes/OProcesses.hpp>
#include "ODeferredExecution.hpp"
#include "ODECriticalSection.hpp"
#include "ODEReturn.hpp"
#include "ODEProcess.hpp"
#include "ODEThread.hpp"
#include "ODEWork.hpp"

#include "../../Processes/OProcessHelpers.hpp"

static error_t APC_AddPendingWork(task_k tsk, ODEWorkHandler * impl);

ODEWorkJobImpl::ODEWorkJobImpl(task_k task, OPtr<OWorkQueue> workqueue)
{
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
    CHK_DEAD;
    _work = work;
    return kStatusOkay;
}

error_t ODEWorkJobImpl::Schedule()
{
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
    CHK_DEAD;
    dispatched = _state.dispatched;
    return kStatusOkay;
}

error_t ODEWorkJobImpl::HasExecuted(bool & executed)
{
    CHK_DEAD;
    executed = _state.execd;
    return kStatusOkay;
}

error_t ODEWorkJobImpl::WaitExecute(uint32_t ms)               
{
    CHK_DEAD;

    return _workqueue->DumbWait(ms);
}

error_t ODEWorkJobImpl::AwaitExecute(ODECompleteCallback_f cb, void * context)
{
    CHK_DEAD;
    if (_callback.func)
        return kErrorInternalError;
    _callback.func = cb;
    _callback.data = context;
    return kStatusOkay;
}

error_t ODEWorkJobImpl::GetResponse(size_t & ret)
{
    CHK_DEAD;
    ret = _state.response;
    return kStatusOkay;
}

ODEWorkHandler * ODEWorkJobImpl::GetWorkObject()
{
    return _worker;
}

void ODEWorkJobImpl::InvalidateImp()
{
    DestoryWorkHandler(this);

    _workqueue->Destroy();

    if (_task)
        ProcessesTaskDecrementCounter(_task);
}

void ODEWorkJobImpl::DeattachWorkObject()
{
    _worker = nullptr;
}

void ODEWorkJobImpl::Trigger(size_t response)
{
    _state.response = response;
    _state.execd    = true;

    _workqueue->Trigger();
}

void ODEWorkJobImpl::GetCallback(ODECompleteCallback_f & callback, void * & context)
{
    callback = _callback.func;
    context  = _callback.data;
}

LIBLINUX_SYM error_t CreateWorkItem(OPtr<OProcessThread> target, const OOutlivableRef<ODEWorkJob> out)
{
    error_t err;
    task_k handle;
    OPtr<OWorkQueue> wq;

    if (!target.GetTypedObject())
        return kErrorIllegalBadArgument;

    err = CreateWorkQueue(1, wq);
    if (ERROR(err))
        return err;

    err = target->GetOSHandle((void **)&handle);
    if (ERROR(err))
        return err;

    if (!out.PassOwnership(new ODEWorkJobImpl(handle, wq)))
        return kErrorOutOfMemory;

    return kStatusOkay;
}

void InitDeferredCalls()
{
    InitDECriticalSection();
    InitDEReturn();
    InitDEProcesses();
    InitDEThreads();
    InitDEWorkHandlers();
}
