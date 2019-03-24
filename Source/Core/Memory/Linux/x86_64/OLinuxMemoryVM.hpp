/*
    Purpose: x86_64 linux memory interfaces
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#pragma once
#include <Core/Memory/Linux/OLinuxMemory.hpp>

class OLMemoryManager
{
public:
    virtual error_t AllocateZone(OLMemoryAllocation * space, size_t start, task_k requester, size_t pages, void ** priv, size_t & ostart, size_t & oend, size_t & length) = 0;
    virtual error_t FreeZone(void * priv) = 0;
    virtual void    SetCallbackHandler(void * priv, OLTrapHandler_f cb, void * context) = 0;

    virtual error_t InsertAt(void * instance, size_t index, void ** map, OLPageEntry entry) = 0;
    virtual error_t RemoveAt(void * instance, void * map) = 0;
};

#include "OLinuxMemoryVMUser.hpp"
#include "OLinuxMemoryVMKernel.hpp"

#include "OLinuxMemoryVMSpace.hpp"
