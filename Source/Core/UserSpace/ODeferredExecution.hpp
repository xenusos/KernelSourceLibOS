/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#pragma once

#include <Core/UserSpace/ODeferredExecution.hpp>

class OLBufferDescription;


class ODEWorkHandler;
class ODEWorkJobImpl : public ODEWorkJob
{
public:
    ODEWorkJobImpl(task_k task);

    error_t SetWork(ODEWork &)                     override;

    error_t Schedule()                             override;

    error_t HasDispatched(bool &)                  override;
    error_t HasExecuted(bool &)                    override;

    error_t WaitExecute(uint32_t ms)               override;
    error_t AwaitExecute(ODECompleteCallback_f cb) override;

    error_t GetResponse(size_t & ret)              override;

    void Hit(size_t response);
    void Fuckoff();
    
protected:
    void InvalidateImp()                           override;

private:
    ODEWorkHandler * _worker;
    ODEWork _work;
    task_k _task;
    bool _execd;
    bool _dispatched;
    size_t _response;
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
    void ParseRegisters(pt_regs & regs);
private:
    ODEWorkJobImpl * _parant;
    ODEWork _work;
    task_k _tsk;
};


LIBLINUX_SYM error_t CreateWorkItem(OPtr<OProcessThread> target, const OOutlivableRef<ODEWorkJobImpl> out);

extern void DeferredExecFinish(size_t ret);
extern void InitDeferredCalls();