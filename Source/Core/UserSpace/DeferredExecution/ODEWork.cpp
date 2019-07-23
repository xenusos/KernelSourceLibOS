/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#include <libos.hpp>
#include "ODEWork.hpp"
#include "ODEThread.hpp"
#include "../../Processes/OProcessHelpers.hpp"
#include "ODeferredExecution.hpp"

static mutex_k work_watcher_mutex;

ODEWorkHandler::ODEWorkHandler(task_k tsk, ODEWorkJobImpl * worker)
{
    _tsk = tsk;
    _parant = worker;
    ProcessesTaskIncrementCounter(_tsk);
}

ODEWorkHandler::~ODEWorkHandler()
{
    if (_tsk)
        ProcessesTaskDecrementCounter(_tsk);
}

void ODEWorkHandler::SetupRegisters(pt_regs & regs)
{
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
        regs.r8 = _work.parameters.three;
        regs.r9 = _work.parameters.four;
    }
}

void ODEWorkHandler::SetupStack(size_t *stack)
{

}

void ODEWorkHandler::DeattachWorkObject()
{
    _parant = nullptr;
}

void ODEWorkHandler::Hit(size_t response)
{
    ODECompleteCallback_f callback = nullptr;
    void * context = nullptr;

    mutex_lock(work_watcher_mutex);
    if (_parant)
    {
        _parant->Trigger(response);
        _parant->GetCallback(callback, context);
        _parant->DeattachWorkObject();
    }
    mutex_unlock(work_watcher_mutex);

    if (callback)
        callback(context);

    delete this;
}

void ODEWorkHandler::Die()
{
    mutex_lock(work_watcher_mutex);
    if (_parant)
    {
        _parant->DeattachWorkObject();
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


void DestoryWorkHandler(ODEWorkJobImpl * job)
{
    mutex_lock(work_watcher_mutex);
    auto worker = job->GetWorkObject();
    if (worker)
        worker->DeattachWorkObject();
    mutex_unlock(work_watcher_mutex);
}

void InitDEWorkHandlers()
{
    work_watcher_mutex = mutex_create();
    ASSERT(work_watcher_mutex, "couldn't allocate mutex");
}
