/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#include <libos.hpp>
#include <Core/UserSpace/ODeferredExecution.hpp>
#include "CCManager.hpp" 
#include "CCIA32Stdcall.hpp"
#include "CCIA32Cdecl.hpp"

ICallingConvention * CCGetIA32StdcallConvention()
{
    return CCGetIA32CdeclConvention(); // we don't clean the stack at all
}
