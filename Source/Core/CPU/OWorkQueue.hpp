/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#include <Core/CPU/OWorkQueue.hpp>

class OSimpleSemaphore;
struct WorkWaitingThreads;
class OWorkQueueImpl : public OWorkQueue
{
public:
    OWorkQueueImpl(uint32_t start_count, mutex_k mutex, dyn_list_head_p list_a, dyn_list_head_p list_b);

    error_t GetCount(uint32_t &)            override;
    error_t EndWork()                       override;
    error_t BeginWork()                     override;
    error_t WaitAndAddOwner(uint32_t ms)    override;
    error_t ReleaseOwner()                  override;

protected:
    void InvalidateImp()                    override;

private:
    error_t GoToSleep(uint32_t ms, bool workers);
    error_t NewThreadContext(WorkWaitingThreads * context, bool waiter);
    error_t ContExecution(bool workers);

    mutex_k _acquisition;
    volatile long _owners;
    volatile long _activeWork;
    volatile long _completed;

    uint32_t _workItems;
    dyn_list_head_p _waiters;
    dyn_list_head_p _workers;
};
