/*
    Purpose: x86_64 linux memory interfaces
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#pragma once
#include <Core/Memory/Linux/OLinuxMemory.hpp>

class OLMemoryInterfaceImpl : public Memory::OLMemoryInterface
{
public:

    Memory::OLPageLocation  GetPageLocation(size_t max)                                                    override;
    size_t                  GetPageRegionStart(Memory::OLPageLocation location)                            override;
    size_t                  GetPageRegionEnd(Memory::OLPageLocation location)                              override;
                                                                                                           
    phys_addr_t             PhysPage(page_k page)                                                          override;
                                                                                                           
    void                    UpdatePageEntryCache(Memory::OLPageEntryMeta &, Memory::OLCacheType cache)     override;
    void                    UpdatePageEntryAccess(Memory::OLPageEntryMeta &, size_t access)                override;
    Memory::OLPageEntryMeta CreatePageEntry(size_t access, Memory::OLCacheType cache)                      override;
                                                                                                           
    error_t GetKernelAddressSpace(const OUncontrollableRef<Memory::OLVirtualAddressSpace> builder)         override;
    error_t GetUserAddressSpace(task_k task, const OOutlivableRef<Memory::OLVirtualAddressSpace> builder)  override;
};

extern Memory::OLMemoryInterface * g_memory_interface;

extern void InitMemmory();

LIBLINUX_SYM error_t Memory::GetLinuxMemoryInterface(const OUncontrollableRef<OLMemoryInterface> interface);
