/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#pragma once

typedef uint32_t los_spinlock_t;

#define SPINLOOP_PROCYIELD() {thread_pause();            }
#define SPINLOOP_SLEEP()     {thread_pause(); msleep(1); }

LIBLINUX_SYM void SpinLock_Init(los_spinlock_t * lock);
LIBLINUX_SYM void SpinLock_Lock(los_spinlock_t * lock);
LIBLINUX_SYM void SpinLock_Unlock(los_spinlock_t * lock);
LIBLINUX_SYM bool SpinLock_IsLocked(los_spinlock_t * lock);