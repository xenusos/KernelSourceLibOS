/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#include <libos.hpp>

#include <Core/CPU/OSpinlock.hpp>
#include <Core/CPU/OThread_CPU.hpp>

#include <ITypes/IThreadStruct.hpp>
#include <ITypes/ITask.hpp>

#include <Utils/DateHelper.hpp>

#include "LinuxSleeping.hpp"

#include "OSemaphore.hpp"

struct SemaWaitingThreads
{
    task_k thread;
    volatile bool signal;
};

OCountingSemaphoreImpl::OCountingSemaphoreImpl(uint32_t startCount, mutex_k mutex, dyn_list_head_p list)
{
    _counter     = startCount;
    _acquisition = mutex;
    _list        = list;
}

error_t OCountingSemaphoreImpl::Wait(uint32_t ms)
{
    CHK_DEAD;
    error_t err;

    mutex_lock(_acquisition);
    {
        if (_counter > 0)
        {
            _counter--;

            err = kStatusSemaphoreAlreadyUnlocked;
            goto out;
        }

        err = GoToSleep(ms);
    }
    out:
    mutex_unlock(_acquisition);

    return err;
}

static bool SemaphoreIsWaking(void * context)
{
    return ((SemaWaitingThreads *)context)->signal;
}

error_t OCountingSemaphoreImpl::NewThreadContext(SemaWaitingThreads * context)
{
    error_t err;
    SemaWaitingThreads **lentry;

    err = dyn_list_append_ex(_list, (void **)&lentry, nullptr);
    if (ERROR(err))
        return err;

    *lentry = context;

    context->thread = OSThread;
    context->signal = false;

    return kStatusOkay;
}

error_t OCountingSemaphoreImpl::GoToSleep(uint32_t ms)
{
    CHK_DEAD;
    error_t err;
    bool signald;
    SemaWaitingThreads entry;

    // create new context
    err = NewThreadContext(&entry);
    if (ERROR(err))
        return err;

    // go to sleep 
    mutex_unlock(_acquisition);
    signald = LinuxSleep(ms, SemaphoreIsWaking, &entry);
    mutex_lock(_acquisition);

    return !signald ? kStatusTimeout  : kStatusOkay;
}

error_t OCountingSemaphoreImpl::ContExecution(uint32_t count, uint32_t & threadsCont)
{
    error_t err;
    SemaWaitingThreads **entry;
    size_t entries;
    size_t threads;

    threadsCont = 0;

    err = dyn_list_entries(_list, &entries);
    if (ERROR(err))
        return err;

    threads = MIN(count, entries);

    for (size_t i = 0; i < threads; i++)
    {
        task_k thread;

        err = dyn_list_get_by_index(_list, 0, (void **)&entry);
        ASSERT(NO_ERROR(err), "couldn't obtain waiting thread by index (error: 0x%zx)", err);

        thread = (*entry)->thread;
        (*entry)->signal = true;
        LinuxPokeThread(thread);

        err = dyn_list_remove(_list, 0);
        ASSERT(NO_ERROR(err), "couldn't remove thread by index (error: 0x%zx)", err);
    }

    threadsCont = threads;
    return kStatusOkay;
}

error_t OCountingSemaphoreImpl::Trigger(uint32_t count, uint32_t & out)
{
    CHK_DEAD;
    uint32_t signals;

    mutex_lock(_acquisition);
    {
        ContExecution(count, signals);

        if (signals != count)
            _counter += count - signals;
    }
    mutex_unlock(_acquisition); 

    return kStatusOkay;
}

void OCountingSemaphoreImpl::InvalidateImp()
{
    error_t err;
    size_t waiters;

    err = dyn_list_entries(_list, &waiters);
    ASSERT(NO_ERROR(err), "couldn't obtain length of waiters (error: 0x%zx)", err);

    ASSERT(waiters == 0, "destoryed counting semaphore with items awaiting");

    dyn_list_destory(_list);
    mutex_destroy(_acquisition);
}

error_t CreateCountingSemaphore(size_t count, const OOutlivableRef<OCountingSemaphore> out)
{
    dyn_list_head_p list;
    mutex_k mutex;
    OSimpleSemaphore * sema;

    if (count > UINT32_MAX)
        return kErrorIllegalSize;

    list = DYN_LIST_CREATE(SemaWaitingThreads*);

    if (!list)
        return kErrorOutOfMemory;

    mutex = mutex_init();

    if (!mutex)
    {
        dyn_list_destory(list);
        return kErrorOutOfMemory;
    }

    if (!out.PassOwnership(new OCountingSemaphoreImpl(count, mutex, list)))
    {
        dyn_list_destory(list);
        mutex_destroy(mutex);
        return kErrorOutOfMemory;
    }

    return kStatusOkay;
}
