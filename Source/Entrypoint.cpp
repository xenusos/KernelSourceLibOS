/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#include <xenus_subsystem.hpp>

#include "Logging/Logging.hpp"

#include "Core/UserSpace/Files/OPseudoFile.hpp"
#include "Core/UserSpace/DeferredExecution/ODeferredExecution.hpp"
#include "Core/UserSpace/DelegatedCalls/ODelegtedCalls.hpp"
#include "Core/Processes/OProcesses.hpp"
#include "Core/Memory/Linux/OLinuxMemory.hpp"
#include "Core/Processes/OProcessTracking.hpp"
#include "Core/UserSpace/ORegistration.hpp"
#include "Core/UserSpace/ODeferredExecution.hpp"
#include "Core/CPU/OThread.hpp"

XENUS_BEGIN_C
    #include <kernel/peloader/pe_loader.h>
XENUS_END_C

static mod_global_data_t module;

#pragma section(".CRT$XCA",long,read)
__declspec(allocate(".CRT$XCA")) void(*__ctors_begin__[1])(void) = { 0 };
#pragma section(".CRT$XCZ",long,read)
__declspec(allocate(".CRT$XCZ")) void(*__ctors_end__[1])(void) = { 0 };

static void libos_constructors_init()
{
    for (void(**ctor)(void) = __ctors_begin__ + 1;
        ctor < __ctors_end__;
        ctor++)
    {
        (*ctor)();
    }
}

static int libos_start()
{
    return 1;
}

static c_bool libos_init(mod_dependency_list_p deps)
{
    libos_constructors_init();

    LoggingInit();
    InitPseudoFiles();
    InitDelegatedCalls();
    InitProcesses();
    InitProcessTracking();
    InitRegistration();
    InitMemmory();
    InitThreading();
    InitDeferredCalls();
    return true;
}

static void libos_shutdown()
{
    
}

void entrypoint(xenus_entrypoint_ctx_p context)
{
    context->size                    = sizeof(xenus_entrypoint_ctx_t);
    context->description             = "LibOS";
    context->copyright               = "All rights reserved, Reece Wilson (2018)";
    context->init                    = libos_init;
    context->start                   = libos_start;
    context->shutdown                = libos_shutdown;
    context->dependencies.count      = 0;
    context->static_data             = &module;
}    
