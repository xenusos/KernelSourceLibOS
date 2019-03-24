/*
    Purpose: x86_64 linux memory interfaces
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#pragma once
#include <Core/Memory/Linux/OLinuxMemory.hpp>

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

extern OLMemoryInterface *          g_memory_interface;

extern void InitMemmory();
