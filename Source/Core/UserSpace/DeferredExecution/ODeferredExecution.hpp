/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#pragma once

#include <Core/CPU/OWorkQueue.hpp> 
#include <Core/UserSpace/ODeferredExecution.hpp>

#define APC_STACK_PAGES CONFIG_APC_STACK_PAGES

class OLMemoryAllocation;

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
    OPtr<OWorkQueue> _workqueue;
    task_k           _task        = {0};
    ODEWorkHandler * _worker      = nullptr;
    ODEWork          _work        = {0};
    struct
    {
        bool   execd;
        bool   dispatched;
        size_t response;
    } _state                      = {0};
    struct
    {
        ODECompleteCallback_f func;
        void * data;
    } _callback                   = {0};
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

private:
    ODEWorkJobImpl * _parant = nullptr;
    ODEWork          _work   = {0};
    task_k           _tsk    = nullptr;
    
};

LIBLINUX_SYM error_t CreateWorkItem(OPtr<OProcessThread> target, const OOutlivableRef<ODEWorkJobImpl> out);

extern void DeferredExecFinish(size_t ret);
extern void InitDeferredCalls();
