/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#include <libos.hpp>
#include <Core/UserSpace/ODeferredExecution.hpp>
#include "CCManager.hpp" 
#include "CCAmd64Microsoft.hpp"
#include "CCAmd64SystemV.hpp"
#include "CCIA32Cdecl.hpp"
#include "CCIA32Stdcall.hpp"

static ICallingConvention * g_calling_conventions[kODEEndConvention];

void ODEInitCallingConventions()
{
    g_calling_conventions[kODEWin64]       = CCGetMSFTConvention();
    g_calling_conventions[kODESysV]        = CCGetSysVConvention();
    g_calling_conventions[kODEIA32Cdecl]   = CCGetIA32CdeclConvention();
    g_calling_conventions[kODEIA32Stdcall] = CCGetIA32StdcallConvention();
}

ICallingConvention * ODEGetConvention(ODECallingConvention cc)
{
    if (cc >= kODEEndConvention)
        return nullptr;
    return g_calling_conventions[cc];
}
