/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#pragma once

namespace Synchronization
{
    class OWorkQueue;
    typedef bool(*SpuriousWakeup_f)(const OPtr<OWorkQueue> & ref); // true = return, false = continue blocking

    static inline bool IsWorkQueueOwner(error_t err)
    {
        return err == kStatusOkay || err == kStatusWorkQueueAlreadyComplete;
    }

    class OWorkQueue : public OObject
    {
    public:
        virtual error_t GetCount(uint32_t &) = 0;

        virtual error_t WaitAndAddOwner(uint32_t ms = -1, SpuriousWakeup_f wakeup = nullptr) = 0;  // check return value against IsWorkQueueOwner(...) to determine ownership
        virtual error_t ReleaseOwner() = 0;

        virtual error_t SpuriousWakeupOwners() = 0;

        virtual error_t EndWork() = 0;
        virtual error_t BeginWork() = 0;

        // Non-reusable APIs, or at least, not as safe. Do not use these unless you're certain that your work load will not break thread safety conditions.
        void Trigger()
        {
            BeginWork();
            EndWork();
        }

        error_t DumbWait(uint32_t ms = -1)
        {
            error_t ret;
            ret = WaitAndAddOwner(ms);
            if (IsWorkQueueOwner(ret))
                ReleaseOwner();
            return ret;
        }
    };

    // A work queue is essentially a reusable work queue with infinite waiters and a set amount of tasks
    // 1. create a work queue with CreateWorkQueue(...)
    // 2. all threads waiting on the task[s] to complete should call WaitAndAddOwner
    // 2.a. all threads that return with kStatusOkay should call ReleaseOwner to give ownership back to the worker thread[s]
    // 2.b  threads that use ms = -1 (async/timeoutable) should check for kStatusTimeout; kStatusTimeout should be treated like an error.
    // 3. all worker threads should call BeginWork to indicate that they're starting to do work 
    // 3.a if a thread hasn't called ReleaseOwner yet, the WaitAndAddOwner callee is assumed to be still processing "everything has completed" logic, therefore the worker thread may go to sleep until such time the owner has been released
    // 4. all worker threads should call EndWork after processing their task
    // 5. goto 2

    // Do note: this is NOT a semaphore
    // A semaphore does not care for reusability or the amount of tasks being processed
    // A semaphore merely allows execution to continue
    // 
    //  ie: Wait                   = while(!AtomicTestAndDecIfHigh(x) { sleep(); }
    //      AtomicTestAndDecIfHigh = {var ret; ret = x; if (x) x--; return ret;}
    //      Trigger                = AtomicIncrement(x) 
    //      AtomicIncrement        = x++
    //
    // This implementation completely differs from such

    LIBLINUX_SYM error_t CreateWorkQueue(size_t work_items, const OOutlivableRef<OWorkQueue> out);
}
