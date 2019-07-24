/*
    Purpose: x86_64 linux memory interfaces
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#pragma once
#include <Core/Memory/Linux/OLinuxMemory.hpp>

class IVMManager
{
public:
    virtual error_t AllocateZone(Memory::OLMemoryAllocation * space, size_t start, task_k requester, size_t pages, void ** priv, size_t & ostart, size_t & oend, size_t & length) = 0;
    virtual error_t FreeZoneMapping(void * priv) = 0;
    virtual void FreeZoneContext(void  * priv) = 0;

    virtual void    SetCallbackHandler(void * priv, Memory::OLTrapHandler_f cb, void * context) = 0;

    virtual error_t InsertAt(void * instance, size_t index, void ** map, Memory::OLPageEntry entry) = 0;
    virtual error_t RemoveAt(void * instance, void * map) = 0;
};
