/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#include <xenus_lazy.h>
#include <libtypes.hpp>
#include <libos.hpp>

#include <Core/CPU/OThread_Spinlocks.hpp>

#include <Core/CPU/OThread_CPU.hpp>

#include <ITypes/IThreadStruct.hpp>
#include <ITypes/ITask.hpp>

#include "OSemaphore.hpp"


#define LOG_MOD "LibOS"
#include <Logging/Logging.hpp>


XENUS_BEGIN_C
long _InterlockedExchangeAdd(
    long volatile *Addend,
    long          Value
);

long _InterlockedDecrement(
    long * lpAddend
);
XENUS_END_C

struct SemaWaitingThreading
{
    task_k thread;
    bool signal;
};

OCountingSemaphoreImpl::OCountingSemaphoreImpl(uint32_t start_count, uint32_t limit, mutex_k mutex, dyn_list_head_p list)
{
    _counter = start_count;
    _limit = limit;
    _acquisition = mutex;
    _list = list;
}

error_t OCountingSemaphoreImpl::GetLimit(size_t & limit)
{
    CHK_DEAD;
    limit = _limit;
    return kStatusOkay;
}

error_t OCountingSemaphoreImpl::GetUsed(size_t & out)
{
    CHK_DEAD;
    out = _counter;
    return kStatusOkay;
}

error_t OCountingSemaphoreImpl::Wait()
{
    CHK_DEAD;
    error_t err;

    mutex_lock(_acquisition);

    if (_counter > 0)
    {
        _InterlockedDecrement(&_counter);
        mutex_unlock(_acquisition);
        return kStatusSemaphoreAlreadyUnlocked;
    }

    if (_counter + 1 > _limit)
    {
        mutex_unlock(_acquisition);
        return kErrorSemaphoreExceededLimit;
    }

    if (NO_ERROR(err = GoToSleep()))
    {
        _InterlockedDecrement(&_counter);
    }

    mutex_unlock(_acquisition);
    return err;
}

error_t OCountingSemaphoreImpl::GoToSleep()
{
    CHK_DEAD;
    SemaWaitingThreading entry;
    SemaWaitingThreading **lentry;
    uint_t ustate;
    error_t err;
    ITask tsk(OSThread);
    size_t idx;

    ustate = tsk.GetVarState().GetUInt();

    if (ERROR(err = dyn_list_append_ex(_list, (void **)&lentry, &idx)))
        return err;

    *lentry = &entry;

    entry.thread = OSThread;
    entry.signal = false;

    while (1)
    {
        // Check if semaphore unlocked
        if (entry.signal)
            break;

        // Sleep
        mutex_unlock(_acquisition);
        {
            tsk.GetVarState().Set((uint_t)TASK_UNINTERRUPTIBLE);
            schedule();
        }
        mutex_lock(_acquisition);
    }

    tsk.GetVarState().Set(ustate);

    return kStatusOkay;
}

error_t OCountingSemaphoreImpl::ContExecution(uint32_t count)
{
    error_t err;
    SemaWaitingThreading **entry;
    size_t entries;
    size_t threads;

    if (ERROR(err = dyn_list_entries(_list, &entries)))
        return err;

    threads = MIN(count, entries);

    for (uint32_t i = 0; i < threads; i++)
    {
        if (ERROR(err = dyn_list_get_by_index(_list, 0, (void **)&entry)))
        {
            LogPrint(kLogError, "couldn't obtain waiting thread... using this semaphore might result in lethal results");
            return err;
        }

        (*entry)->signal = true;
        wake_up_process((*entry)->thread);

        if (ERROR(err = dyn_list_remove(_list, 0)))
        {
            LogPrint(kLogError, "couldn't obtain remove thread entry... using this semaphore might result in lethal results");
            return err;
        }
    }
    
    return kStatusOkay;
}

error_t OCountingSemaphoreImpl::Trigger(uint32_t count, uint32_t & out)
{
    CHK_DEAD;
    uint32_t next;

    mutex_lock(_acquisition);
    next = _InterlockedExchangeAdd(&_counter, count);
    
    if (next > _limit)
    {
        _counter -= count; // f for atomicy 
        mutex_unlock(_acquisition);
        return kErrorIllegalSize;
    }
    else
    {
        ContExecution(count);

        mutex_unlock(_acquisition);
        return kStatusOkay;
    }
}

void OCountingSemaphoreImpl::InvaildateImp()
{
    dyn_list_destory(_list);
    mutex_destroy(_acquisition);
}

error_t CreateCountingSemaphore(size_t count, size_t limit, const OOutlivableRef<OCountingSemaphore> out)
{
    dyn_list_head_p list;
    mutex_k mutex;
    OSimpleSemaphore * sema;

    if (count > UINT32_MAX)
        return kErrorIllegalSize;

    if (limit > UINT32_MAX)
        return kErrorIllegalSize;

    list = DYN_LIST_CREATE(SemaWaitingThreading*);

    if (!list)
        return kErrorOutOfMemory;

    mutex = mutex_init();

    if (!mutex)
    {
        dyn_list_destory(list);
        return kErrorOutOfMemory;
    }

    if (!out.PassOwnership(new OCountingSemaphoreImpl(count, limit, mutex, list)))
    {
        dyn_list_destory(list);
        mutex_destroy(mutex);
        return kErrorOutOfMemory;
    }

    return kStatusOkay;
}