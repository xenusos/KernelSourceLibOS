#pragma once

class OCountingSemaphore : public  OObject
{
public:
    virtual error_t GetLimit(size_t &)           = 0;
    virtual error_t GetUsed(size_t &)            = 0;
    virtual error_t Wait()                       = 0;
    virtual error_t Trigger(uint32_t count, uint32_t & out)  = 0;
};

LIBLINUX_SYM error_t CreateCountingSemaphore(size_t count, size_t limit, const OOutlivableRef<OCountingSemaphore> out);