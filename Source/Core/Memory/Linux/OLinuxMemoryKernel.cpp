/*
    Purpose: x86_64 linux memory interfaces
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#define DANGEROUS_PAGE_LOGIC
#include <libos.hpp>
#include "OLinuxMemory.hpp"
#include "OLinuxMemoryMM.hpp"

#include "../../Processes/OProcesses.hpp"

OLMemoryManagerKernel g_krnvm_manager;

static l_unsigned_long page_offset_base = 0;
static page_k          kernel_dummy_page;
static sysv_fptr_t     ioremap_page_range;

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

    area = __get_vm_area(length, 0, kernel_information.LINUX_VMALLOC_START, kernel_information.LINUX_VMALLOC_END);
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

error_t OLMemoryManagerKernel::FreeZone(void * priv)
{
    AddressSpaceKernelContext * context = (AddressSpaceKernelContext *)priv;

    vunmap((const void *)context->address);
    delete context;
}

error_t OLMemoryManagerKernel::InsertAt(void * instance, size_t index, void ** map, OLPageEntry entry)
{
    // eqiv to vmap()
    AddressSpaceKernelContext * context;
    int ret;
    size_t adr;
    size_t end;

    context = (AddressSpaceKernelContext *)instance;

    adr = context->address + (index << OS_PAGE_SHIFT);
    end = adr + OS_PAGE_SIZE;

    if (entry.type == kPageEntryByAddress)
    {
        // TODO: 
        // The Linux kernel would usually
        // 1. check entry protection (boring) 
        // 2. check against request_resource (boring... i live life on the edge)
        // 3. update the linear maps cache/pcm to match entry.prot (fucking why?)
        // 4. allocate the vm address
        // 5. map
        // we should do all of those things, but for now, let's just do 4-5

        // TODO: add this to sysv
        //
        //int ioremap_page_range(unsigned long addr, unsigned long end, phys_addr_t phys_addr, pgprot_t prot)
        // QWORD, QWORD, PHYSADDR/QWORD, pgprot_t (QWORD)

        ret = (l_int)ez_linux_caller(ioremap_page_range, adr, end, (size_t)entry.address, entry.prot.pgprot_, 0, 0, 0, 0, 0, 0, 0, 0);
       
        if (ret != 0) // 0 on OK
            return kErrorInternalError;
    }
    else
    {
        page_k page;
        pgprot_t prot;

        if (entry.type == kPageEntryByPage)
        {
            page = entry.page;
            prot = entry.prot;
        }
        else if (entry.type == kPageEntryDummy)
        {
            page = kernel_dummy_page;
            prot = g_memory_interface->CreatePageEntry(0, kCacheNoCache).prot;
        }

        ret = map_kernel_range_noflush(adr, OS_PAGE_SIZE, prot, &page);
        // x86 doesn't require flush 

        if (ret != 1) // ret = page injected
            return kErrorInternalError;

    }
    
    *map = (void *)adr;
    return kStatusOkay;
}

error_t OLMemoryManagerKernel::RemoveAt(void * instance, void * map)
{
    unmap_kernel_range_noflush((size_t)map, OS_PAGE_SIZE);
}

page_k * OLKernelVirtualAddressSpaceImpl::AllocatePages(OLPageLocation location, size_t cnt, bool contig, size_t flags)
{
    return AllocateLinuxPages(location, cnt, false, contig, flags);
}

void OLKernelVirtualAddressSpaceImpl::FreePages(page_k * pages)
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

error_t  OLKernelVirtualAddressSpaceImpl::MapPage(page_k page, size_t pages, size_t & address, void * & context)
{
    return MapPhys(pfn_to_phys(page_to_pfn(page)), pages, address, context);
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
    page_offset_base = *(l_unsigned_long*)kallsyms_lookup_name("page_offset_base");
    ioremap_page_range = (sysv_fptr_t) kallsyms_lookup_name("ioremap_page_range");
    kernel_dummy_page = alloc_pages_current(GFP_KERNEL, 0);
}
