/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#include <libos.hpp>
#include "OWorkQueue.hpp"

#include <Utils/DateHelper.hpp>
#include <ITypes/IThreadStruct.hpp>
#include <ITypes/ITask.hpp>
#include "LinuxSleeping.hpp"

struct WorkWaitingThreads
{
    Synchronization::SpuriousWakeup_f wakeup;
    Synchronization::OWorkQueue * queue;
    task_k thread;
    bool signal;
};

OWorkQueueImpl::OWorkQueueImpl(uint32_t workItems, mutex_k mutex, dyn_list_head_p listWorkers, dyn_list_head_p listWaiters)
{
    _activeWork  = 0;
    _completed   = 0;
    _owners      = 0;
    _workItems   = workItems;
    _acquisition = mutex;
    _waiters     = listWorkers;
    _workers     = listWaiters;
}

error_t OWorkQueueImpl::GetCount(uint32_t & out)
{
    CHK_DEAD;
    out = _completed;
    return kStatusOkay;
}

error_t OWorkQueueImpl::WaitAndAddOwner(uint32_t ms, Synchronization::SpuriousWakeup_f wakeup)
{
    CHK_DEAD;
    error_t err;

    mutex_lock(_acquisition);

    _owners++;

    if (_completed == _workItems)
    {
        err = kStatusWorkQueueAlreadyComplete;
        goto out;
    }

    err = GoToSleep(ms, wakeup, true);

    if (ERROR(err) || (err == kStatusTimeout))
    {
        _owners--;
    }

    out:
    mutex_unlock(_acquisition);
    return err;
}

static bool WorkerThreadIsWaking(void * context)
{
    auto ctx = reinterpret_cast <WorkWaitingThreads *>(context);
    return ctx->signal || (ctx->wakeup && ctx->wakeup(ctx->queue));
}

error_t OWorkQueueImpl::NewThreadContext(WorkWaitingThreads * context, Synchronization::SpuriousWakeup_f wakeup, bool waiters)
{
    CHK_DEAD;
    error_t err;
    dyn_list_head_p list;
    WorkWaitingThreads ** lentry;

    list = waiters ? _waiters : _workers;

    context->thread = OSThread;
    context->signal = false;
    context->wakeup = wakeup;
    context->queue  = this;

    err = dyn_list_append_ex(list, (void **)&lentry, nullptr);
    if (ERROR(err))
        return err;

    *lentry = context;
    return kStatusOkay;
}

error_t OWorkQueueImpl::GoToSleep(uint32_t ms, Synchronization::SpuriousWakeup_f wakeup, bool waiters)
{
    CHK_DEAD;
    WorkWaitingThreads entry;
    error_t err;
    bool signald;

    // create new context
    err = NewThreadContext(&entry, wakeup, waiters);
    if (ERROR(err))
        return err;

    // go to sleep 
    mutex_unlock(_acquisition);
    signald = LinuxSleep(ms, WorkerThreadIsWaking, &entry);
    mutex_lock(_acquisition);
    
    return !signald ? kStatusTimeout : kStatusOkay;
}

error_t OWorkQueueImpl::ContExecution(bool waiters)
{
    CHK_DEAD;
    error_t err;
    size_t threads;
    dyn_list_head_p list;
    WorkWaitingThreads ** entry;

    list = waiters ? _waiters : _workers;

    err = dyn_list_entries(list, &threads);
    if (ERROR(err))
        return err;

    for (size_t i = 0; i < threads; i++)
    {
        task_k thread;

        err = dyn_list_get_by_index(list, 0, reinterpret_cast<void **>(&entry));
        ASSERT(NO_ERROR(err), "couldn't obtain waiting thread by index (error: " PRINTF_ERROR ")", err);

        thread = (*entry)->thread;
        (*entry)->signal = true;
        LinuxPokeThread(thread);

        // TODO: remove entry on spurious exit 
        err = dyn_list_remove(list, 0);
        ASSERT(NO_ERROR(err), "couldn't remove thread by index (error: " PRINTF_ERROR ")", err);
    }
    
    return kStatusOkay;
}

error_t OWorkQueueImpl::BeginWork()
{
    CHK_DEAD;
    uint32_t next;

    mutex_lock(_acquisition);
    {
        if (_activeWork == _workItems) // note: active workers never decrements - it only resets to zero once all owners have been released 
            GoToSleep(-1, NULL, false);
     
        _activeWork++; 
    }
    mutex_unlock(_acquisition);

    return kStatusOkay;
}

error_t OWorkQueueImpl::EndWork()
{
    CHK_DEAD;

    mutex_lock(_acquisition);
    {
        if ((++_completed) == _workItems)
            ContExecution(true);
    }
    mutex_unlock(_acquisition);

    return kStatusOkay;
}

error_t OWorkQueueImpl::SpuriousWakeupOwners()
{
    CHK_DEAD;
    error_t err;
    size_t threads;
    WorkWaitingThreads ** entry;

    mutex_lock(_acquisition);

    err = dyn_list_entries(_waiters, &threads);
    if (ERROR(err))
        return err;

    for (size_t i = 0; i < threads; i++)
    {
        task_k thread;

        err = dyn_list_get_by_index(_waiters, 0, reinterpret_cast<void **>(&entry));
        ASSERT(NO_ERROR(err), "couldn't obtain waiting thread by index (error: " PRINTF_ERROR ")", err);

        thread = (*entry)->thread;
        LinuxPokeThread(thread);
    }

    mutex_unlock(_acquisition);
    return kStatusOkay;
}

error_t OWorkQueueImpl::ReleaseOwner()
{
    CHK_DEAD;
    error_t err = kStatusOkay;

    mutex_lock(_acquisition);
    {
        if (_owners == 0)
        {
            LogPrint(kLogError, "OWorkQueueImpl::ReleaseOwner - someone tried to release an owner that doesn't exist");
            err = kErrorTooManyReleases;
            goto out;
        }

        if ((--_owners) == 0)
        {
            _activeWork = 0;
            _completed  = 0;
            ContExecution(false);
        }

    }
    out:
    mutex_unlock(_acquisition);

    return err;
}

void OWorkQueueImpl::InvalidateImp()
{
    error_t err;
    size_t count;

    err = dyn_list_entries(_workers, &count);
    ASSERT(NO_ERROR(err), "couldn't obtain length of waiters (error: " PRINTF_ERROR ")", err);
    ASSERT(count == 0, "Destroyed work queue with work threads waiting");

    err = dyn_list_entries(_waiters, &count);
    ASSERT(NO_ERROR(err), "couldn't obtain length of job dispatchers (error: " PRINTF_ERROR ")", err);
    ASSERT(count == 0, "Destroyed work queue with job dispatcher threads waiting");

    dyn_list_destroy(_workers);
    dyn_list_destroy(_waiters);
    mutex_destroy(_acquisition);
}

error_t Synchronization::CreateWorkQueue(size_t cont, const OOutlivableRef<Synchronization::OWorkQueue> out)
{
    dyn_list_head_p listPrimary;
    dyn_list_head_p listSecondary;
    mutex_k mutex;
    OSimpleSemaphore * sema;

    if (cont > UINT32_MAX)
        return kErrorIllegalSize;

    listPrimary = DYN_LIST_CREATE(WorkWaitingThreads*);

    if (!listPrimary)
        return kErrorOutOfMemory;

    mutex = mutex_init();

    if (!mutex)
    {
        dyn_list_destroy(listPrimary);
        return kErrorOutOfMemory;
    }

    listSecondary = DYN_LIST_CREATE(WorkWaitingThreads*);

    if (!listSecondary)
    {
        dyn_list_destroy(listPrimary);
        mutex_destroy(mutex);
        return kErrorOutOfMemory;
    }

    if (!out.PassOwnership(new OWorkQueueImpl(cont, mutex, listSecondary, listPrimary)))
    {
        dyn_list_destroy(listPrimary);
        dyn_list_destroy(listSecondary);
        mutex_destroy(mutex);
        return kErrorOutOfMemory;
    }

    return kStatusOkay;
}
