/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson
*/  
#include <xenus_lazy.h>
#include <libos.hpp>

#include <Core/CPU/OThread.hpp>

XENUS_BEGIN_C
extern unsigned char _interlockedbittestandset(
    long *a,
    long b
);
XENUS_END_C

void SpinLock_Lock(los_spinlock_t * lock)
{
    while (_interlockedbittestandset((long *)lock, 0))
    {
        while (*lock)
        {
            SPINLOOP_BLOCK();
        }
    }
}

void SpinLock_Unlock(los_spinlock_t * lock)
{
    *lock = 0;
}

void SpinLock_Init(los_spinlock_t * lock)
{
    SpinLock_Unlock(lock);
}

bool SpinLock_IsLocked(los_spinlock_t * lock)
{
    return (bool) *lock;
}