/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#include <libos.hpp>

#include <Core/CPU/OThread_Spinlocks.hpp>

#include "OWorkQueue.hpp"

#include <ITypes/IThreadStruct.hpp>
#include <ITypes/ITask.hpp>

#define LOG_MOD "LibOS"
#include <Logging/Logging.hpp>

const long  HZ = 300; // TODO: Get from OSINFO

static inline long MS_TO_JIFFIES(long ms)
{
    if (ms > 1000)
        return HZ * ms / 1000;
    else
        return HZ / (1000 / ms);
}

struct WorkWaitingThreads
{
    task_k thread;
    bool signal;
};

OWorkQueueImpl::OWorkQueueImpl(uint32_t start_count, mutex_k mutex, dyn_list_head_p list_a, dyn_list_head_p list_b)
{
    _counter = 0;
    _completed = 0;
    _trigger_on = start_count;
    _acquisition = mutex;
    _waiters = list_a;
    _workers = list_b;
}


error_t OWorkQueueImpl::GetCount(uint32_t & out)
{
    CHK_DEAD;
    out = _completed;
    return kStatusOkay;
}

error_t OWorkQueueImpl::WaitAndAddOwner(uint32_t ms)
{
    CHK_DEAD;
    error_t err;

    mutex_lock(_acquisition);

    _InterlockedIncrement(&_owners);

    if (_completed == _trigger_on)
    {
        mutex_unlock(_acquisition);
        return kStatusOkay;
    }

    err = GoToSleep(ms, true);

    if (ERROR(err) || (err == kStatusTimeout))
    {
        _InterlockedDecrement(&_owners);
    }

    mutex_unlock(_acquisition);
    return err;
}

error_t OWorkQueueImpl::GoToSleep(uint32_t ms, bool waiters)
{
    CHK_DEAD;
    WorkWaitingThreads entry;
    WorkWaitingThreads **lentry;
    uint_t ustate;
    error_t err;
    ITask tsk(OSThread);
    size_t idx;
    int64_t timeout;
    bool timeoutable;

    ustate = tsk.GetVarState().GetUInt();

    if (ERROR(err = dyn_list_append_ex(waiters ? _waiters : _workers, (void **)&lentry, &idx)))
        return err;

    *lentry = &entry;

    entry.thread = OSThread;
    entry.signal = false;

    if (ms != -1)
    {
        timeout = MAX(1, MS_TO_JIFFIES(ms));
        timeoutable = true;
    }
    else
    {
        timeoutable = false;
    }

    while (1)
    {
        // Check if semaphore unlocked
        if (entry.signal)
            break;

        if ((timeoutable) && (timeout == 0))
            break;

        // Sleep
        mutex_unlock(_acquisition);
        {
            if (!timeoutable)
            {
                tsk.GetVarState().Set((uint_t)TASK_INTERRUPTIBLE);
                schedule();
            }
            else
            {
                timeout = schedule_timeout_interruptible(timeout);
            }
        }
        mutex_lock(_acquisition);
    }

    tsk.GetVarState().Set(ustate);
    
    if ((timeoutable) && (timeout == 0))
        return kStatusTimeout;
    
    return kStatusOkay;
}

error_t OWorkQueueImpl::ContExecution(bool waiters)
{
    error_t err;
    WorkWaitingThreads **entry;
    size_t threads;
    dyn_list_head_p list;

    list = waiters ? _waiters : _workers;

    if (ERROR(err = dyn_list_entries(list, &threads)))
        return err;

    for (uint32_t i = 0; i < threads; i++)
    {
        if (ERROR(err = dyn_list_get_by_index(list, 0, (void **)&entry)))
        {
            LogPrint(kLogError, "couldn't obtain waiting thread... using this semaphore might result in lethal results");
            return err;
        }

        (*entry)->signal = true;
        wake_up_process((*entry)->thread);

        if (ERROR(err = dyn_list_remove(list, 0)))
        {
            LogPrint(kLogError, "couldn't obtain remove thread entry... using this semaphore might result in lethal results");
            return err;
        }
    }
    
    return kStatusOkay;
}

error_t OWorkQueueImpl::BeginWork()
{
    CHK_DEAD;
    uint32_t next;
    mutex_lock(_acquisition);
    if (_counter == _trigger_on)
        GoToSleep(-1, false);
    _InterlockedIncrement(&_counter); // x++ should be atomic, but i dont trust msvc
    mutex_unlock(_acquisition);
    return kStatusOkay;
}

error_t OWorkQueueImpl::EndWork()
{
    CHK_DEAD;
    mutex_lock(_acquisition);
    if (_InterlockedIncrement(&_completed) == _trigger_on)
        ContExecution(true);
    mutex_unlock(_acquisition);
    return kStatusOkay;
}

error_t OWorkQueueImpl::ReleaseOwner()
{
    CHK_DEAD;
    size_t owners;

    mutex_lock(_acquisition);
    owners = _owners;

    if (owners == 0)
    {
        mutex_unlock(_acquisition);
        return kErrorTooManyReleases;
    }

    _owners = --owners;

    if (owners == 0)
    {
        _counter   = 0;
        _completed = 0;
        ContExecution(false);
    }
    mutex_unlock(_acquisition);
    return kStatusOkay;
}

void OWorkQueueImpl::InvalidateImp()
{
    dyn_list_destory(_workers);
    dyn_list_destory(_waiters);
    mutex_destroy(_acquisition);
}

error_t CreateWorkQueue(size_t cont, const OOutlivableRef<OWorkQueue> out)
{
    dyn_list_head_p list;
    dyn_list_head_p list_a;
    mutex_k mutex;
    OSimpleSemaphore * sema;

    if (cont > UINT32_MAX)
        return kErrorIllegalSize;

    list = DYN_LIST_CREATE(WorkWaitingThreads*);

    if (!list)
        return kErrorOutOfMemory;

    mutex = mutex_init();

    if (!mutex)
    {
        dyn_list_destory(list);
        return kErrorOutOfMemory;
    }

    list_a = DYN_LIST_CREATE(WorkWaitingThreads*);

    if (!list_a)
    {
        dyn_list_destory(list);
        mutex_destroy(mutex);
        return kErrorOutOfMemory;
    }

    if (!out.PassOwnership(new OWorkQueueImpl(cont, mutex, list_a, list)))
    {
        dyn_list_destory(list);
        dyn_list_destory(list_a);
        mutex_destroy(mutex);
        return kErrorOutOfMemory;
    }

    return kStatusOkay;
}

