/*
    Purpose: x86_64 linux memory interfaces
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#define DANGEROUS_PAGE_LOGIC
#include <libos.hpp>
#include "OLinuxMemoryVM.hpp"

#include "OLinuxMemoryMM.hpp"
#include "OLinuxMemoryPages.hpp"
#include "../OLinuxMemory.hpp"

OLMemoryManagerKernel g_krnvm_manager;;

static l_unsigned_long page_offset_base = 0;
static page_k          kernel_dummy_page;

struct AddressSpaceKernelContext
{
    size_t address;
    size_t length;
    size_t pages;
    vm_struct_k area;
};

error_t OLMemoryManagerKernel::AllocateZone(OLMemoryAllocation * space, size_t start, task_k requester, size_t pages, void ** priv, size_t & ostart, size_t & oend, size_t & olength)
{
    vm_struct_k area;
    size_t length;
    AddressSpaceKernelContext * context;
    
    length = pages << OS_PAGE_SHIFT;

    context = new AddressSpaceKernelContext();
    if (!context)
        return kErrorOutOfMemory;

    area = __get_vm_area(length, 0x00000001 | 0x00000040, kernel_information.LINUX_VMALLOC_START, kernel_information.LINUX_VMALLOC_END);
    if (!area)
    {
        delete context;
        return kErrorOutOfMemory;
    }

    context->area = area;
    context->pages = pages;
    context->length = length;
    context->address = (size_t) area->addr;

    *priv = context;

    ostart  = context->address;
    oend    = ostart + length;
    olength = length;
    return kStatusOkay;
}

void OLMemoryManagerKernel::FreeZoneContext(void * priv)
{
    delete reinterpret_cast<AddressSpaceKernelContext *>(priv);
}

error_t OLMemoryManagerKernel::FreeZoneMapping(void * priv)
{
    vunmap(reinterpret_cast<const void *>(reinterpret_cast<AddressSpaceKernelContext *>(priv)->address));
    return kStatusOkay;
}

error_t OLMemoryManagerKernel::InsertAt(void * instance, size_t index, void ** map, OLPageEntry entry)
{
    AddressSpaceKernelContext * context;
    int ret;
    size_t adr;
    size_t end;
    phys_addr_t phys;
    page_k page;
    pfn_t pfn;
    pgprot_t prot;

    context = (AddressSpaceKernelContext *)instance;

    adr = context->address + (index << OS_PAGE_SHIFT);
    end = adr + OS_PAGE_SIZE;

    switch (entry.type)
    {
    case kPageEntryByAddress:
    {
        page = nullptr;
        phys = entry.address;
        pfn  = phys_to_pfn(entry.address);
        prot = entry.meta.kprot;
        break;
    }
    case kPageEntryByPFN:
    {
        page = nullptr;
        phys = pfn_to_phys(entry.pfn);
        pfn  = entry.pfn;
        prot = entry.meta.kprot;
        break;
    }
    case kPageEntryByPage:
    {
        page = entry.page;
        prot = entry.meta.kprot;
        pfn  = page_to_pfn(page);
        phys = pfn_to_phys(pfn);
        break;
    }
    case kPageEntryDummy:
    {
        page = kernel_dummy_page;
        prot = g_memory_interface->CreatePageEntry(0, kCacheNoCache).kprot;
        pfn  = page_to_pfn(page);
        phys = pfn_to_phys(pfn);
        break;
    }
    default:
    {
        panic("unhandled page type");
    }
    }

    if (page)
    {
        ret = map_kernel_range_noflush(adr, OS_PAGE_SIZE, prot, &page);

        if (ret != 1) // page count on OK
            return kErrorInternalError;
    }
    else
    {
        // TODO: 
        // The Linux kernel would usually
        // 1. check entry protection (boring) 
        // 2. check against request_resource (boring... i live life on the edge)
        // 3. update the linear maps cache/pcm to match entry.prot w/ some special processor stuff
        // 4. allocate the vm address
        // 5. map
        // we should do all of those things, but for now, let's just do 4-5

        ret = kernel_map_sync_memtype((uint64_t) phys, OS_PAGE_SIZE, GetCacheModeFromCacheType(entry.meta.cache));

        if (ret != 0) // 0 on OK
            return kErrorInternalError;

        ret = ioremap_page_range(adr, end, phys, prot);

        if (ret != 0) // 0 on OK
            return kErrorInternalError;
    }

    *map = (void *)adr;
    return kStatusOkay;
}

error_t OLMemoryManagerKernel::RemoveAt(void * instance, void * map)
{
    unmap_kernel_range_noflush((size_t)map, OS_PAGE_SIZE);
    return kStatusOkay;
}

PhysAllocationElem * OLKernelVirtualAddressSpaceImpl::AllocatePages(OLPageLocation location, size_t cnt, bool contig, size_t flags)
{
    return AllocateLinuxPages(location, cnt, false, contig, false, flags);
}

PhysAllocationElem * OLKernelVirtualAddressSpaceImpl::AllocatePFNs(OLPageLocation location, size_t cnt, bool contig, size_t flags)
{
    return AllocateLinuxPages(location, cnt, false, contig, true, flags);
}

void OLKernelVirtualAddressSpaceImpl::FreePages(PhysAllocationElem * pages)
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

error_t OLKernelVirtualAddressSpaceImpl::NewDescriptor(size_t start, size_t pages, const OOutlivableRef<OLMemoryAllocation> allocation)
{
    error_t ret;
    OLMemoryAllocation * instance;
   
    ret = GetNewMemAllocation(true, nullptr, start, pages, instance);
    if (ERROR(ret))
        return ret;

    allocation.PassOwnership(instance);
    return kStatusOkay;
}

void InitKernVMMemory()
{
    page_offset_base   = *(l_unsigned_long*) kallsyms_lookup_name("page_offset_base");
    kernel_dummy_page  = alloc_pages_current(GFP_KERNEL, 0);
}
