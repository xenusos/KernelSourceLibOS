/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson
*/  

#include <xenus_subsystem.hpp>

#include "Logging/Logging.hpp"

#include "Core/UserSpace/OPseudoFile.hpp"
#include "Core/UserSpace/ODelegtedCalls.hpp"
#include "Core/Processes/OProcesses.hpp"
#include "Core/Memory/Linux/OLinuxMemory.hpp"
#include "Core/Processes/OProcessTracking.hpp"
#include "Core/UserSpace/ORegistration.hpp"
#include "Core/UserSpace/ODeferredExecution.hpp"
#include "Core/CPU/OThread.hpp"

XENUS_BEGIN_C
#include <kernel/peloader/pe_loader.h>
XENUS_END_C

mod_global_data_t module;

int libos_start()
{
    return 1;
}

c_bool libos_init(mod_dependency_list_p deps)
{
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

void libos_shutdown()
{
    
}

void entrypoint(xenus_entrypoint_ctx_p context)
{
    context->size					= sizeof(xenus_entrypoint_ctx_t);
    context->description			= "LibOS";
    context->copyright				= "All rights reserved, Reece Wilson (2018)";
    context->init					= libos_init;
    context->start					= libos_start;
    context->shutdown               = libos_shutdown;
    context->dependencies.count		= 0;
    context->static_data			= &module;
}