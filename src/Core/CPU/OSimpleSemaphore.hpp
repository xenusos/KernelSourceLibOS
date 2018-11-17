/*
    Purpose: near-deprecated wait-queue implementation
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/

class OSimpleSemaphore : public OObject
{
public:
    OSimpleSemaphore(size_t counter, dyn_list_head_p list);

    error_t GetCount(uint32_t &);
    error_t Trigger(uint32_t &);
    error_t Wait(uint32_t &);
    error_t Reuse();

protected:
    void InvaildateImp();
    void ContExecution();
	
private:
    long _counter;
    //linked_list_head_p _list;
    dyn_list_head_p _head;
    los_spinlock_t _lock;
    long _waiting;
    long _ogcont;
    bool _dead;
};

error_t CreateSimpleSemaphore(size_t cont, const OOutlivableRef<OSimpleSemaphore> out);