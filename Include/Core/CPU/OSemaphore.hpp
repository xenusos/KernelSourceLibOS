/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#pragma once

class OCountingSemaphore : public  OObject
{
public:
    virtual error_t Wait(uint32_t ms = -1)                                                = 0;
    virtual error_t Trigger(uint32_t count, uint32_t & releasedThreads, uint32_t & debt)  = 0;
};

LIBLINUX_SYM error_t CreateCountingSemaphore(size_t count, const OOutlivableRef<OCountingSemaphore> out);
