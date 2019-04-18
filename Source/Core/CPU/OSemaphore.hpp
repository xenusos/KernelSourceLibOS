/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#include <Core/CPU/OSemaphore.hpp>

class OSimpleSemaphore;
struct SemaWaitingThreads;

class OCountingSemaphoreImpl : public OCountingSemaphore
{
public:
    OCountingSemaphoreImpl(uint32_t start_count, mutex_k mutex, dyn_list_head_p list);
    error_t Wait(uint32_t ms)                                                    override;
    error_t Trigger(uint32_t count, uint32_t & releasedThreads, uint32_t & debt) override;

protected:
    void InvalidateImp()                                                         override;

private:
    error_t GoToSleep(uint32_t ms);
    error_t NewThreadContext(SemaWaitingThreads * context);
    error_t ContExecution(uint32_t count, uint32_t & threadsCont);

    mutex_k _acquisition;
    volatile long _counter;
    dyn_list_head_p _list;
};
