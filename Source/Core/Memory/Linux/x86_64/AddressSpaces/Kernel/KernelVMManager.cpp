/*
    Purpose: x86_64 linux memory interfaces
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#include "Common.hpp"
#include "../IVMManager.hpp"
#include "KernelVMManager.hpp"

IVMManagerKernel g_krnvm_manager;;

static l_unsigned_long page_offset_base = 0;
static page_k          kernel_dummy_page;

struct AddressSpaceKernelContext
{
    size_t address;
    size_t length;
    size_t pages;
    vm_struct_k area;
};

error_t IVMManagerKernel::AllocateZone(Memory::OLMemoryAllocation * space, size_t start, task_k requester, size_t pages, void ** priv, size_t & ostart, size_t & oend, size_t & olength)
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

void IVMManagerKernel::FreeZoneContext(void * priv)
{
    delete reinterpret_cast<AddressSpaceKernelContext *>(priv);
}

error_t IVMManagerKernel::FreeZoneMapping(void * priv)
{
    vunmap(reinterpret_cast<const void *>(reinterpret_cast<AddressSpaceKernelContext *>(priv)->address));
    return kStatusOkay;
}

error_t IVMManagerKernel::InsertAt(void * instance, size_t index, void ** map, Memory::OLPageEntry entry)
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
    case Memory::kPageEntryByAddress:
    {
        page = nullptr;
        phys = entry.address;
        pfn  = phys_to_pfn(entry.address);
        prot = entry.meta.kprot;
        break;
    }
    case Memory::kPageEntryByPFN:
    {
        page = nullptr;
        phys = pfn_to_phys(entry.pfn);
        pfn  = entry.pfn;
        prot = entry.meta.kprot;
        break;
    }
    case Memory::kPageEntryByPage:
    {
        page = entry.page;
        prot = entry.meta.kprot;
        pfn  = page_to_pfn(page);
        phys = pfn_to_phys(pfn);
        break;
    }
    case Memory::kPageEntryDummy:
    {
        page = kernel_dummy_page;
        prot = g_memory_interface->CreatePageEntry(0, Memory::kCacheNoCache).kprot;
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

error_t IVMManagerKernel::RemoveAt(void * instance, void * map)
{
    unmap_kernel_range_noflush((size_t)map, OS_PAGE_SIZE);
    return kStatusOkay;
}

void InitKernVMMemory()
{
    page_offset_base = *(l_unsigned_long*)kallsyms_lookup_name("page_offset_base");
    kernel_dummy_page = alloc_pages_current(GFP_KERNEL, 0);
}
