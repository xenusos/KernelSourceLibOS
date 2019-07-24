/*
    Purpose: x86_64 linux memory interfaces
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#define DANGEROUS_PAGE_LOGIC
#include "Common.hpp"
#include "UserAddressSpace.hpp"
#include "../VMAllocation.hpp"
#include <Source/Core/Processes/OProcessHelpers.hpp>

OLUserVirtualAddressSpaceImpl::OLUserVirtualAddressSpaceImpl(task_k task)
{
    if (!task)
    {
        LogPrint(kLogWarning, "OLUserspaceAddressSpace constructor called with null process instance... using OSThread (a/k/a current)");
        task = OSThread;
    }

    _task = task;

    ProcessesTaskIncrementCounter(_task);
}

void OLUserVirtualAddressSpaceImpl::InvalidateImp()
{
    ProcessesTaskDecrementCounter(_task);
}

Memory::PhysAllocationElem * OLUserVirtualAddressSpaceImpl::AllocatePages(Memory::OLPageLocation location, size_t cnt, bool contig, size_t flags)
{
    return AllocateLinuxPages(location, cnt, true, contig, false, flags);
}

Memory::PhysAllocationElem * OLUserVirtualAddressSpaceImpl::AllocatePFNs(Memory::OLPageLocation location, size_t cnt, bool contig, size_t flags)
{
    return AllocateLinuxPages(location, cnt, true, contig, true, flags);
}

void OLUserVirtualAddressSpaceImpl::FreePages(Memory::PhysAllocationElem * pages)
{
    FreeLinuxPages(pages);
}

error_t  OLUserVirtualAddressSpaceImpl::MapPhys(phys_addr_t phys, size_t pages, size_t & address, void * & context)
{
    // TODO:
    return kErrorNotImplemented;
}

error_t  OLUserVirtualAddressSpaceImpl::UnmapPhys(void * context)
{
    // TODO:
    return kErrorNotImplemented;
}

error_t  OLUserVirtualAddressSpaceImpl::MapPage(page_k page, size_t & address, void * & context)
{
    // TODO:
    return kErrorNotImplemented;
}

error_t  OLUserVirtualAddressSpaceImpl::UnmapPage(void * context)
{
    // TODO:
    return kErrorNotImplemented;
}

error_t OLUserVirtualAddressSpaceImpl::NewDescriptor(size_t start, size_t pages, const OOutlivableRef<Memory::OLMemoryAllocation> allocation)
{
    error_t ret;
    Memory::OLMemoryAllocation * instance;

    ret = GetNewMemAllocation(false, _task, start, pages, instance);
    if (ERROR(ret))
        return ret;

    allocation.PassOwnership(instance);
    return kStatusOkay;
}
