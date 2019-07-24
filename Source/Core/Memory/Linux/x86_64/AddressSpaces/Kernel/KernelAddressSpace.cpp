/*
    Purpose: x86_64 linux memory interfaces
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#include "Common.hpp"
#include "KernelAddressSpace.hpp"
#include "../VMAllocation.hpp"

Memory::PhysAllocationElem * OLKernelVirtualAddressSpaceImpl::AllocatePages(Memory::OLPageLocation location, size_t cnt, bool contig, size_t flags)
{
    return AllocateLinuxPages(location, cnt, false, contig, false, flags);
}

Memory::PhysAllocationElem * OLKernelVirtualAddressSpaceImpl::AllocatePFNs(Memory::OLPageLocation location, size_t cnt, bool contig, size_t flags)
{
    return AllocateLinuxPages(location, cnt, false, contig, true, flags);
}

void OLKernelVirtualAddressSpaceImpl::FreePages(Memory::PhysAllocationElem * pages)
{
    FreeLinuxPages(pages);
}

error_t  OLKernelVirtualAddressSpaceImpl::MapPhys(phys_addr_t phys, size_t pages, size_t & address, void * & context)
{
    address = phys_to_virt(phys);
    context = (void *)0xBEEFCA3ECA3ECA3E;
    return kStatusOkay;
}

error_t  OLKernelVirtualAddressSpaceImpl::UnmapPhys(void * context)
{
    if (size_t(context) != 0xBEEFCA3ECA3ECA3E)
        return kErrorIllegalBadArgument;
 
    return kStatusOkay;
}

error_t  OLKernelVirtualAddressSpaceImpl::MapPage(page_k page, size_t & address, void * & context)
{
    return MapPhys(pfn_to_phys(page_to_pfn(page)), 1, address, context);
}

error_t  OLKernelVirtualAddressSpaceImpl::UnmapPage(void * context)
{
    return UnmapPhys(context);
}

error_t OLKernelVirtualAddressSpaceImpl::NewDescriptor(size_t start, size_t pages, const OOutlivableRef<Memory::OLMemoryAllocation> allocation)
{
    error_t ret;
    Memory::OLMemoryAllocation * instance;
   
    ret = GetNewMemAllocation(true, nullptr, start, pages, instance);
    if (ERROR(ret))
        return ret;

    allocation.PassOwnership(instance);
    return kStatusOkay;
}

