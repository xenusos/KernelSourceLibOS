/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#pragma once

#include "OThread_Preemption.hpp"
//#include "OSpinlock.hpp"
#include "OThread_CPU.hpp"
#include "OThread_Nice.h"
#include "OThread_Timing.hpp"

#if defined(TGT_KRN_LINUX)
    #include "OThread_Linux.hpp"
#endif 

class OThread : public OObject
{
public:
    virtual error_t GetExitCode(int64_t &)        = 0;

    virtual error_t GetThreadId(uint32_t & id)    = 0;

    virtual error_t IsAlive(bool &)               = 0; 
    virtual error_t IsRunning(bool &)             = 0;    
                                                  
    virtual error_t IsMurderable(bool &)          = 0; 
    virtual error_t TryMurder(long exit)          = 0; 

    virtual error_t GetPOSIXNice(int32_t & nice)  = 0;
    virtual error_t SetPOSIXNice(int32_t nice)    = 0;
                                                 
    virtual error_t GetName(const char *& str)    = 0;

    virtual error_t IsFloatingHandle(bool &)      = 0;
    virtual error_t GetOSHandle(void *& handle)   = 0;

    virtual void * GetData() = 0;
};

LIBLINUX_SYM bool UtilityIsTask32Bit(task_k handle);

#include "OThread_Spawning.hpp"
