/*
    Purpose: x86_64 linux memory interfaces
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#pragma once


class OLUserVirtualAddressSpaceImpl : public Memory::OLVirtualAddressSpace
{
public:

    OLUserVirtualAddressSpaceImpl(task_k task);

    Memory::PhysAllocationElem * AllocatePFNs(Memory::OLPageLocation location, size_t cnt, bool contig, size_t flags)         override;
    Memory::PhysAllocationElem * AllocatePages(Memory::OLPageLocation location, size_t cnt, bool contig, size_t flags)         override;
    void                 FreePages(Memory::PhysAllocationElem * pages)                                             override;

    error_t  MapPhys(phys_addr_t phys, size_t pages, size_t & address, void * & context)                 override;
    error_t  UnmapPhys(void * context)                                                                     override;

    error_t  MapPage(page_k page, size_t & address, void * & context)                                    override;
    error_t  UnmapPage(void * context)                                                                     override;

    error_t NewDescriptor(size_t start, size_t pages, const OOutlivableRef<Memory::OLMemoryAllocation> allocation)     override;

protected:
    void InvalidateImp() override;

private:

    task_k _task;
};
