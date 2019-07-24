/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#include <Core/Synchronization/OWorkQueue.hpp>

class OSimpleSemaphore;
struct WorkWaitingThreads;
class OWorkQueueImpl : public Synchronization::OWorkQueue
{
public:
    OWorkQueueImpl(uint32_t start_count, mutex_k mutex, dyn_list_head_p list_a, dyn_list_head_p list_b);

    error_t GetCount(uint32_t &)                                     override;
    error_t EndWork()                                                override;
    error_t BeginWork()                                              override;
    error_t WaitAndAddOwner(uint32_t ms, Synchronization::SpuriousWakeup_f wakeup)    override;
    error_t ReleaseOwner()                                           override;
    error_t SpuriousWakeupOwners()                                   override;

protected:
    void InvalidateImp()                                             override;

private:
    error_t GoToSleep(uint32_t ms, Synchronization::SpuriousWakeup_f wakeup, bool workers);
    error_t NewThreadContext(WorkWaitingThreads * context, Synchronization::SpuriousWakeup_f wakeup, bool waiters);
    error_t ContExecution(bool workers);

    mutex_k _acquisition;
    volatile long _owners;
    volatile long _activeWork;
    volatile long _completed;

    uint32_t _workItems;
    dyn_list_head_p _waiters;
    dyn_list_head_p _workers;
};

LIBLINUX_SYM error_t Synchronization::CreateWorkQueue(size_t cont, const OOutlivableRef<Synchronization::OWorkQueue> out);
