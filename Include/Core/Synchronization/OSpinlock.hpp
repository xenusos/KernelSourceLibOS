/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#pragma once

namespace Synchronization
{
    class LIBLINUX_CLS Spinlock
    {
    public:
        Spinlock();

        void Lock();
        void Unlock();
        bool IsLocked();
    private:
        long _value;
    };
}

#define SPINLOOP_PROCYIELD() {thread_pause();            }
#define SPINLOOP_SLEEP()     {thread_pause(); msleep(1); }
