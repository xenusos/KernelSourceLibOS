/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#pragma once

class OWorkQueue : public  OObject
{
public:
    virtual error_t GetCount(uint32_t &)   = 0;
    
    virtual error_t WaitAndAddOwner(uint32_t ms)    = 0; // if kStatusTimeout or kStatusSemaphoreAlreadyUnlocked, you do not own 
    virtual error_t ReleaseOwner()       = 0;

    virtual error_t EndWork()            = 0;
    virtual error_t BeginWork()          = 0;

    void Trigger()
    {
        BeginWork();
        EndWork();
    }
};

LIBLINUX_SYM error_t CreateWorkQueue(size_t work_items, const OOutlivableRef<OWorkQueue> out);
