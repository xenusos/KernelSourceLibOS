/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#pragma once

class ICallingConvention
{
public:
    virtual void SetupRegisters(const ODEWork & work, pt_regs & regs) = 0;
    virtual size_t * SetupStack(const ODEWork & work, size_t * stack) = 0;
};

extern void ODEInitCallingConventions();
extern ICallingConvention * ODEGetConvention(ODECallingConvention cc);
