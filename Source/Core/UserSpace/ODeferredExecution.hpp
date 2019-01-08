/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#pragma once

#include <Core/CPU/OWorkQueue.hpp> 
#include <Core/UserSpace/ODeferredExecution.hpp>

#define APC_STACK_PAGES CONFIG_APC_STACK_PAGES

class OLBufferDescription;

class ODEWorkHandler;
class ODEWorkJobImpl : public ODEWorkJob
{
public:
    ODEWorkJobImpl(task_k task, OPtr<OWorkQueue> workqueue);
                                                                   
    error_t SetWork(ODEWork &)                                     override;
                                                                   
    error_t Schedule()                                             override;
                                                                   
    error_t HasDispatched(bool &)                                  override;
    error_t HasExecuted(bool &)                                    override;
                                                                   
    error_t WaitExecute(uint32_t ms)                               override;
    error_t AwaitExecute(ODECompleteCallback_f cb, void * context) override;
                                                                   
    error_t GetResponse(size_t & ret)                              override;

    void GetCallback(ODECompleteCallback_f & callback, void * & context);
    void Trigger(size_t response);
    void Fuckoff();                                                
                                                                   
protected:                                                         
    void InvalidateImp()                                           override;
                                                                   
private:
    ODEWorkHandler * _worker;
    ODEWork _work;
    task_k _task;
    bool _execd;
    bool _dispatched;
    size_t _response;
    OPtr<OWorkQueue> _workqueue;
    ODECompleteCallback_f _cb;
    void * _cb_ctx;
};

struct APCStack
{
    page_k pages[APC_STACK_PAGES];
    size_t length;
    struct
    {
        union
        {
            size_t address;
            size_t bottom;
        };
        size_t top;
    } mapped;
};

class ODEWorkHandler 
{
public:
    ODEWorkHandler(task_k tsk, ODEWorkJobImpl * work);
    ~ODEWorkHandler();

    error_t SetWork(ODEWork & work);
    error_t Schedule();
    void Fuckoff();
    void Hit(size_t response);
    void Die();
    void ParseRegisters(pt_regs & regs);

    error_t AllocateStack();
    error_t AllocateStub();
    error_t MapToKernel();
    error_t Construct();
private:
    ODEWorkJobImpl * _parant;
    ODEWork _work;
    task_k _tsk;
    
    APCStack _stack;

    struct
    {
        size_t   sp;
        size_t   address;
        OPtr<OLBufferDescription> desc;
    } _kernel_map; 

    size_t _rtstub;
};


LIBLINUX_SYM error_t CreateWorkItem(OPtr<OProcessThread> target, const OOutlivableRef<ODEWorkJobImpl> out);

extern void DeferredExecFinish(size_t ret);
extern void InitDeferredCalls();