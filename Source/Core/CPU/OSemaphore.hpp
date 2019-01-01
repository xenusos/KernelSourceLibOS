/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/

#include <Core/CPU/OSemaphore.hpp>

class OSimpleSemaphore;

class OCountingSemaphoreImpl : public OCountingSemaphore
{
public:
    OCountingSemaphoreImpl(uint32_t start_count, uint32_t limit, mutex_k mutex, dyn_list_head_p list);
    error_t GetLimit(size_t &)                      override;
    error_t GetUsed(size_t &)                       override;
    error_t Wait(uint32_t ms)                       override;
    error_t Trigger(uint32_t count, uint32_t & out) override;

protected:
    void InvalidateImp()                            override;

private:
    error_t GoToSleep(uint32_t ms);
    error_t ContExecution(uint32_t count);

    mutex_k _acquisition;
    long _counter;
    uint32_t _limit;
    dyn_list_head_p _list;
};
