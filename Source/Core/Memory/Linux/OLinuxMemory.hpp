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
    virtual error_t FreeZone(void * priv)                                                                  = 0;
    virtual void    SetCallbackHandler(void * priv, OLTrapHandler_f cb, void * context)                    = 0;
    
    virtual error_t InsertAt(void * instance, size_t index, void ** map, OLPageEntry entry)                = 0;
    virtual error_t RemoveAt(void * instance, void * map)                                                  = 0;
};

class OLMemoryInterfaceImpl : public OLMemoryInterface
{
public:

    OLPageLocation  GetPageLocation(size_t max)                                                            override;
    size_t          GetPageRegionStart(OLPageLocation location)                                            override;
    size_t          GetPageRegionEnd(OLPageLocation location)                                              override;


    phys_addr_t     PhysPage(page_k page)                                                                  override;

    void            UpdatePageEntryCache(OLPageEntryMeta &, OLCacheType cache)                             override;
    void            UpdatePageEntryAccess(OLPageEntryMeta &, size_t access)                                override;
    OLPageEntryMeta CreatePageEntry(size_t access, OLCacheType cache)                                      override;

    error_t GetKernelAddressSpace(const OUncontrollableRef<OLVirtualAddressSpace> builder)                 override;
    error_t GetUserAddressSpace(task_k task, const OOutlivableRef<OLVirtualAddressSpace> builder)          override;
};


#include "OLinuxMemoryUser.hpp"
#include "OLinuxMemoryKernel.hpp"
#include "OLinuxMemoryAddressSpace.hpp"

extern OLMemoryInterface *          g_memory_interface;

extern void InitMemmory();
