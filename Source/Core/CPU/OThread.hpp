/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson
*/  
#pragma once
#include <Core/CPU/OThread.hpp>

class OThreadImp : public OThread
{
public:
    OThreadImp(task_k tsk, uint32_t id, const char * name, const void * data);

    error_t GetExitCode(int64_t &)                    override;

    error_t GetThreadId(uint32_t & id)                override;

    error_t IsAlive(bool &)                           override;
    error_t IsRunning(bool &)                         override;    

    error_t IsMurderable(bool &)                      override;
    error_t TryMurder(long exit)                      override;

    error_t GetPOSIXNice(int32_t & nice)              override;
    error_t SetPOSIXNice(int32_t nice)                override;

    error_t GetName(const char *& str)                override;

    error_t IsFloatingHandle(bool &)                  override;      
    error_t GetOSHandle(void *& handle)               override;

    void SignalDead(long exitcode = -420);

    void * GetData() override;

protected:
    void InvalidateImp()                              override;

    char _name[100];
    uint32_t _id;
    task_k _tsk;
    const void * _data;
    long _exit_code;
    bool _try_kill;
};

void InitThreading();
