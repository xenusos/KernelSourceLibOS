/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#include <libos.hpp>
#include <Core/UserSpace/ODeferredExecution.hpp>
#include "CCManager.hpp" 
#include "CCIA32Cdecl.hpp"

class CdeclConvention : public ICallingConvention
{
    void SetupRegisters(const ODEWork & work, pt_regs & regs) override;
    size_t * SetupStack(const ODEWork & work, size_t * stack) override;
};

void CdeclConvention::SetupRegisters(const ODEWork & work, pt_regs & regs)
{
    regs.rip = work.address;
}

size_t *  CdeclConvention::SetupStack(const ODEWork & work, size_t * stack)
{
    uint32_t * id32stack = reinterpret_cast<uint32_t *>(stack);

    *id32stack-- = work.parameters.four;
    *id32stack-- = work.parameters.three;
    *id32stack-- = work.parameters.two;
    *id32stack-- = work.parameters.one;

    return reinterpret_cast<size_t *>(id32stack);
}

ICallingConvention * CCGetIA32CdeclConvention()
{
    static CdeclConvention  interface;
    return dynamic_cast<ICallingConvention *>(&interface);
}
