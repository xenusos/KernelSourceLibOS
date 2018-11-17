/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/

#include <Core/CPU/OWorkQueue.hpp>

class OSimpleSemaphore;
class OWorkQueueImpl : public OWorkQueue
{
public:
    OWorkQueueImpl(uint32_t count, OSimpleSemaphore * counter, mutex_k b);

    error_t GetCount(uint32_t &) override;
    error_t EndWork() override;
    error_t BeginWork() override;
    error_t WaitAndAddOwner() override;
    error_t ReleaseOwner() override;
    

protected:
    void InvaildateImp();

private:
    //OSimpleSemaphore * _owners;
    mutex_k _work_protect; // locked during work, unlocked while waiters are doing their thing
    OSimpleSemaphore * _lock;
    long _o_counter;
    long _count;
    los_spinlock_t _spinlock;
};