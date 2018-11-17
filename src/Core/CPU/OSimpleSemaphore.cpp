/*
    Purpose: near-deprecated wait-queue implementation
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#include <xenus_lazy.h>
#include <libtypes.hpp>
#include <libos.hpp>

#include <Core/CPU/OThread_Spinlocks.hpp>

#include "OSimpleSemaphore.hpp"

#include <Core/CPU/OThread_CPU.hpp>

#include <ITypes/IThreadStruct.hpp>
#include <ITypes/ITask.hpp>

XENUS_BEGIN_C
long _InterlockedDecrement(
    long * lpAddend
);

long _InterlockedIncrement(
    long * lpAddend
);
XENUS_END_C

OSimpleSemaphore::OSimpleSemaphore(size_t counter, dyn_list_head_p list)
{
    _head = list;
    _counter = counter;
    _dead = false;
    _ogcont = counter;
    _waiting = 0;
    SpinLock_Init(&_lock);
}

error_t OSimpleSemaphore::GetCount(uint32_t & out)
{
    CHK_DEAD;
    out = _counter;
    return kStatusOkay;
}

error_t OSimpleSemaphore::Trigger(uint32_t & out)
{
    CHK_DEAD;
    error_t err;

    err = kStatusOkay;

    SpinLock_Lock(&_lock);

    out = _InterlockedDecrement(&_counter);
    if (out == 0)
    {
        ContExecution();
    }
    else if (out == -1)
    {
        _counter = 0;
        err = kErrorSemaphoreAlreadyUnlocked;
    }

    SpinLock_Unlock(&_lock);

    return err;
}

error_t OSimpleSemaphore::Wait(uint32_t & out)
{
    CHK_DEAD;
    uint_t ustate;
    ITask tsk(OSThread);
    task_k * entry;
    error_t err;
    size_t index;
    
    ustate = tsk.GetVarState().GetUInt();
    
    SpinLock_Lock(&_lock); // protect _counter and the list of waiting threads
    
    if (_counter == 0)
    {
        SpinLock_Unlock(&_lock);
        return kStatusSemaphoreAlreadyUnlocked;
    }
    
    if (_dead)
    {
        SpinLock_Unlock(&_lock);
        return kStatusSemaphoreAlreadyUnlocked;
    }
    
    if (ERROR(err = dyn_list_append_ex(_head, (void **)&entry, &index)))
    {
        SpinLock_Unlock(&_lock);
        return err;
    }

    *entry = OSThread;
    out = _InterlockedIncrement(&_waiting);
    
    SpinLock_Unlock(&_lock);
    
    while (1)
    {
        // Check if semaphore unlocked
        ThreadingMemoryFlush();
        if (_counter == 0) 
            break;
    
        // Sleep
        tsk.GetVarState().Set((uint_t)TASK_UNINTERRUPTIBLE);
        schedule();
    }
    
    tsk.GetVarState().Set(ustate);
    
    if (_InterlockedDecrement(&_waiting) == 0)
        _dead = true;

    return kStatusOkay;
}

error_t OSimpleSemaphore::Reuse()
{
    CHK_DEAD;

    if (_waiting != 0)
        return kErrorGenericFailure;

    _counter = _ogcont;
    _dead    = false;
    return kStatusOkay;
}

void OSimpleSemaphore::ContExecution()
{
    dyn_list_iterate(_head, [](void * buffer, void *)
    {
        task_k * tsk = (task_k *)buffer;
        wake_up_process(*tsk);
    }, nullptr);
    dyn_list_reset(_head);
}

void OSimpleSemaphore::InvaildateImp()
{
    if (!_dead)
    {
        ASSERT(_counter == 0, "Destoried semaphore with a counter of %zu and %zu threads waiting", _counter, _waiting);
    }
}

error_t CreateSimpleSemaphore(size_t cont, const OOutlivableRef<OSimpleSemaphore> out)
{
    dyn_list_head_p list;
    OSimpleSemaphore * sema;

    if (cont > UINT32_MAX)
        return kErrorIllegalSize;

    list = DYN_LIST_CREATE(task_k);

    if (!list)
        return kErrorOutOfMemory;

    if (!out.PassOwnership(new OSimpleSemaphore(cont, list)))
    {
        dyn_list_destory(list);
        return kErrorOutOfMemory;
    }

    return kStatusOkay;
}