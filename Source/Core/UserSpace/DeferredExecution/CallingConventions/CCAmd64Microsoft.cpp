/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#include <libos.hpp>
#include <Core/UserSpace/ODeferredExecution.hpp>
#include "CCManager.hpp" 
#include "CCAmd64Microsoft.hpp"

class MSFTConvention : public ICallingConvention
{
    void SetupRegisters(const ODEWork & work, pt_regs & regs) override;
    size_t * SetupStack(const ODEWork & work, size_t * stack) override;
};

void MSFTConvention::SetupRegisters(const ODEWork & work, pt_regs & regs)
{
    regs.rip = work.address;
    regs.rcx = work.parameters.one;
    regs.rdx = work.parameters.two;
    regs.r8  = work.parameters.three;
    regs.r9  = work.parameters.four;
}

size_t * MSFTConvention::SetupStack(const ODEWork & work, size_t * stack)
{
    return stack;
}

ICallingConvention * CCGetMSFTConvention()
{
    static MSFTConvention interface;
    return dynamic_cast<ICallingConvention *>(&interface);
}
