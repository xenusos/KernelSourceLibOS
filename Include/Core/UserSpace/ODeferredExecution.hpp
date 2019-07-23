/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#pragma once
class OProcessThread;

struct ODEParameters
{
    size_t one;
    size_t two;
    size_t three;
    size_t four;
};

enum ODECallingConvention
{
    kODESysV = 0,
    kODEWin64,
    kODEIA32Cdecl,
    kODEIA32Stdcall,
    kODEEndConvention
};

struct ODEWork
{
    size_t address;
    ODECallingConvention cc;
    ODEParameters parameters;
};

typedef void(* ODECompleteCallback_f)(void * context);

class ODEWorkJob : public OObject
{
public:
    virtual error_t SetWork(ODEWork &)                                     = 0;
                                                                           
    virtual error_t Schedule()                                             = 0;
                                                                           
    virtual error_t HasDispatched(bool &)                                  = 0;
    virtual error_t HasExecuted(bool &)                                    = 0;
                                                                           
    virtual error_t WaitExecute(uint32_t ms = -1)                          = 0; // > 1 waiters allowed
    virtual error_t AwaitExecute(ODECompleteCallback_f cb, void * context) = 0; // only one call back is allowed

    virtual error_t GetResponse(size_t & ret)                              = 0;
};

LIBLINUX_SYM error_t CreateWorkItem(OPtr<OProcessThread> target, const OOutlivableRef<ODEWorkJob> out);
