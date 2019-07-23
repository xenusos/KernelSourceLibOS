/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#include <libos.hpp>
#include <Core/UserSpace/ODeferredExecution.hpp>
#include "CCManager.hpp" 
#include "CCAmd64SystemV.hpp"

class SysVConvention : public ICallingConvention
{
    void SetupRegisters(const ODEWork & work, pt_regs & regs) override;
    size_t * SetupStack(const ODEWork & work, size_t * stack) override;
};

void SysVConvention::SetupRegisters(const ODEWork & work, pt_regs & regs)
{
    regs.rip = work.address;
    regs.rdi = work.parameters.one;
    regs.rsi = work.parameters.two;
    regs.rdx = work.parameters.three;
    regs.rcx = work.parameters.four;
}

size_t * SysVConvention::SetupStack(const ODEWork & work, size_t * stack)
{
    return stack;
}

ICallingConvention * CCGetSysVConvention()
{
    static SysVConvention  interface;
    return dynamic_cast<ICallingConvention *>(&interface);
}
