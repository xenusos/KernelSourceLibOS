/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#include <xenus_lazy.h>
#include <libtypes.hpp>
#include <libos.hpp>

#include <Core/CPU/OThread_Spinlocks.hpp>

#include "OSimpleSemaphore.hpp"
#include "OWorkQueue.hpp"

XENUS_BEGIN_C
long _InterlockedDecrement(
    long * lpAddend
);

long _InterlockedIncrement(
    long * lpAddend
);
XENUS_END_C

OWorkQueueImpl::OWorkQueueImpl(uint32_t count, OSimpleSemaphore * counter, mutex_k b)
{
    uint32_t ignored;
    _lock = counter;
    _o_counter = 0;
    _count = count;
    _work_protect = b;
    SpinLock_Init(&_spinlock);
    mutex_lock(_work_protect); // locked during work, unlocked while waiters are doing their thing
                               // defacto allow work
                               // using a simplesemaphore with a count of one is excessive for this application
}

error_t OWorkQueueImpl::GetCount(uint32_t & cnt)
{
    CHK_DEAD;
    return _lock->GetCount(cnt);
}

error_t OWorkQueueImpl::BeginWork()
{
    CHK_DEAD;
    error_t err;
    uint32_t cnt;

    SpinLock_Lock(&_spinlock); 
    
    if (ERROR(err = _lock->GetCount(cnt)))
    {
        SpinLock_Unlock(&_spinlock);
        return err;
    }

    if (cnt == 0)
        mutex_lock(_work_protect);

    SpinLock_Unlock(&_spinlock);
    return kStatusOkay;
}

error_t OWorkQueueImpl::EndWork()
{
    CHK_DEAD;
    uint32_t cnt;
    return _lock->Trigger(cnt);
}

error_t OWorkQueueImpl::WaitAndAddOwner()
{
    CHK_DEAD;
    uint32_t cont;
    error_t er;

    cont = _InterlockedIncrement(&_o_counter);

    if (ERROR(er = _lock->Wait(cont)))
    {
        _InterlockedDecrement(&_o_counter);
        return er;
    }

    return kStatusOkay;
}

error_t OWorkQueueImpl::ReleaseOwner()
{
    uint32_t ignored;
    CHK_DEAD;
    if (_InterlockedDecrement(&_o_counter) == 0)
    {
        mutex_unlock(_work_protect);
        ASSERT(NO_ERROR(_lock->Reuse()),            "OWorkQueueImpl::ReleaseOwner() couldn't reset lock semaphore");
    }
    return kStatusOkay;
}

void OWorkQueueImpl::InvaildateImp()
{
    _lock->Destory();
    mutex_destroy(_work_protect);
}

error_t CreateWorkQueue(size_t cont, const OOutlivableRef<OWorkQueue> out)
{
    error_t err;
    OSimpleSemaphore * sema_lock;
    mutex_k b;

    if (ERROR(err = CreateSimpleSemaphore(cont, sema_lock)))
    {
        return err;
    }

    if (!(b = mutex_init()))
    {
        sema_lock->Destory();
        return err;
    }

    if (!(out.PassOwnership(new OWorkQueueImpl(cont, sema_lock, b))))
    {
        sema_lock->Destory();
        mutex_destroy(b);
        return kErrorOutOfMemory;
    }

    return kStatusOkay;
}
