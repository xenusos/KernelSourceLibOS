/*
    Purpose: x86_64 linux memory interfaces
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#include <xenus_lazy.h>
#include <libtypes.hpp>
#include <libos.hpp>

#include "OLinuxMemory.hpp"

#include "../../Processes/OProcesses.hpp"

#define LOG_MOD "LibOS"
#include <Logging/Logging.hpp>

static l_unsigned_long page_offset_base = 0;
static OLMemoryInterface * linux_memory = 0;

#define MEMORY_DEVICE "XenusMemoryMapper"

void * special_map_fault;
void * special_map_handle;
vm_special_mapping_k special_map;

DEFINE_SYSV_FUNCTON_START(special_map_fault, size_t)
const vm_special_mapping_k sm,
vm_area_struct_k vma,
vm_fault_k vmf,
void * pad,
DEFINE_SYSV_FUNCTON_END_DEF(special_map_fault, size_t)
{

    SYSV_FUNCTON_RETURN(0)
}
DEFINE_SYSV_END

OLKernelMappedBufferImpl::OLKernelMappedBufferImpl()
{
    _map = nullptr;
}

size_t OLKernelMappedBufferImpl::GetVAStart()
{
    return size_t(_map);
}

size_t OLKernelMappedBufferImpl::GetVAEnd()
{
    return GetVAStart() + GetLength();
}

size_t OLKernelMappedBufferImpl::GetLength()
{
    return _length;
}

error_t OLKernelMappedBufferImpl::Unmap()
{
    if (_map)
    {
        vunmap(_map);
        _map = nullptr;
    }

    return kStatusOkay;
}

error_t OLKernelMappedBufferImpl::Map(dyn_list_head_p pages, pgprot_t prot)
{
    page_k *entry;
    size_t cnt;
    error_t ret;
    void * map;

    if (ERROR(ret = dyn_list_entries(pages, &cnt)))
        return ret;

    if (ERROR(ret = dyn_list_get_by_index(pages, 0, (void **)&entry)))
        return ret;

    for (size_t i = 0; i < cnt; i++)
    {
        if (!entry[i])
        {
            LogPrint(LoggingLevel_e::kLogError, "Found NULL page entry in descriptor, idx %lli. You can't do this bullshit in the kernel; usermode is fine & will cause an internal trap, kernel would crash", i);
            return kErrorGenericFailure;
        }
    }

    map = vmap(entry, cnt, 0, prot);

    if (!map)
        return kErrorInternalError;

    _length = PAGE_SIZE * cnt;
    _map = map;

    return kStatusOkay;
}

void OLKernelMappedBufferImpl::InvaildateImp()
{
    Unmap();
}

OLUserMappedBufferImpl::OLUserMappedBufferImpl()
{
    _length = 0;
    _map = 0;
    _mm = nullptr;
}

size_t OLUserMappedBufferImpl::GetVAStart()
{
    return _map;
}

size_t OLUserMappedBufferImpl::GetVAEnd()
{
    return GetVAStart() + GetLength();
}

size_t OLUserMappedBufferImpl::GetLength()
{
    return _length;
}

error_t OLUserMappedBufferImpl::Map(dyn_list_head_p pages, task_k task, pgprot_t prot)
{
    page_k *entry;
    size_t cnt;
    error_t ret;
    size_t map;
    mm_struct_k mm;
    vm_area_struct_k area;

    if (ERROR(ret = dyn_list_entries(pages, &cnt)))
        return ret;

    if (ERROR(ret = dyn_list_get_by_index(pages, 0, (void **)&entry)))
        return ret;

    for (size_t i = 0; i < cnt; i++)
    {
        if (!entry[i])
        {
            LogPrint(LoggingLevel_e::kLogError, "Found NULL page entry in descriptor, idx %lli", i);
            return kErrorGenericFailure;
        }
    }

    ProcessesLockTask(task);
    mm = (mm_struct_k)task_get_mm_size_t(task);

    if (!mm)
    {
        ProcessesUnlockTask(task);
        return kErrorInternalError;
    }

    down_write(mm_struct_get_mmap_sem(mm));
    ProcessesMMLock(mm);
    ProcessesUnlockTask(task);

    map = (size_t)get_unmapped_area(NULL, 0, cnt << OS_PAGE_SHIFT, 0, 0);
   
    if (!map)
    {
        up_write(mm_struct_get_mmap_sem(mm));
        ProcessesMMUnlock(mm);
        return kErrorInternalError;
    }

    vm_special_mapping_set_fault_size_t(special_map, size_t(special_map_fault));
    vm_special_mapping_set_name_size_t(special_map,  size_t("XenusMemoryArea"));

    area = _install_special_mapping(mm, (l_unsigned_long)map, cnt << OS_PAGE_SHIFT, VM_READ | VM_WRITE | VM_MAYWRITE | VM_MAYREAD | VM_MAYWRITE, special_map);
    // TODO: area may leak
    if (!area)
    {
        up_write(mm_struct_get_mmap_sem(mm));
        ProcessesMMUnlock(mm);
        return kErrorInternalError;
    }

    vm_area_struct_set_vm_page_prot_uint64(area, prot.pgprot_);

    for (size_t i = 0; i < cnt; i++)
    {
        if (!entry[i])
            continue;

        ASSERT(vm_insert_page(area, (l_unsigned_long)map + (i << OS_PAGE_SHIFT), entry[i]) == 0, "fatal error occurred while inserting user page");
        // TODO: assert
    }
    
    up_write(mm_struct_get_mmap_sem(mm));

    _length = PAGE_SIZE * cnt;
    _map = map;
    _mm = mm;

    return kStatusOkay;
}

error_t OLUserMappedBufferImpl::Unmap()
{
    if (_map && _mm)
    {
        vm_munmap_ex(_mm, _map, _length);
        ProcessesMMUnlock(_mm);
        _map = 0;
        _mm = nullptr;
    }
    return kStatusOkay;
}

void OLUserMappedBufferImpl::InvaildateImp()
{
    Unmap();
}

OLBufferDescriptionImpl::OLBufferDescriptionImpl()
{
    _mapped = nullptr;
}

error_t OLBufferDescriptionImpl::Construct() //we can't allocate memory in a constructor... shit
{
    _pages = DYN_LIST_CREATE(page_k);

    if (!_pages)
        return kErrorOutOfMemory;

    return kStatusOkay;
}

bool    OLBufferDescriptionImpl::PageIsPresent(int idx)
{
    void * buf;

    if (!_pages)
        return false;

    if (ERROR(dyn_list_get_by_index(_pages, idx, &buf)))
        return false;

    return true;
}

error_t OLBufferDescriptionImpl::PageInsert(int idx, page_k page)
{
    page_k *entry;
    size_t cnt;
    error_t ret;

    if (!_pages)
        return kErrorOutOfMemory;

    if (_mapped)
        return kErrorAlreadyMapped;

    if (ERROR(ret = dyn_list_entries(_pages, &cnt)))
        return ret;

    if (cnt == idx)
    {
        if (ERROR(ret = dyn_list_append(_pages, (void **)&entry)))
            return ret;
    }
    else
    {
        if (ERROR(ret = dyn_list_get_by_index(_pages, idx, (void **)&entry)))
            return ret;
    }

    *entry = page;

    return kStatusOkay;
}

error_t OLBufferDescriptionImpl::PagePhysAddr(int idx, phys_addr_t & addr)
{
    error_t err;
    page_k *entry;

    if (!_pages)
        return false;

    if (ERROR(dyn_list_get_by_index(_pages, idx, (void **)&entry)))
        return false;

    if (!(*entry))
        return kErrorPageNotFound;

    addr = linux_memory->PhysPage(*entry);
    return addr ? kStatusOkay : XENUS_ERROR_OS_UNSUPPORTED;
}

error_t OLBufferDescriptionImpl::PageMap(int idx, void * & addr)
{
    error_t err;
    page_k *entry;

    if (!_pages)
        return false;

    if (ERROR(dyn_list_get_by_index(_pages, idx, (void **)&entry)))
        return false;

    if (!(*entry))
        return kErrorPageNotFound;
    
    addr = linux_memory->MapPage(*entry);
    return addr ? kStatusOkay : XENUS_ERROR_OS_UNSUPPORTED;
}

void OLBufferDescriptionImpl::PageUnmap(void * addr)
{
    return linux_memory->UnmapPage(addr);
}

bool OLBufferDescriptionImpl::IsVoid()
{
    return false;
}

error_t OLBufferDescriptionImpl::HasError()
{
    return kFuckMe; //formerly XENUS_STATUS_NOT_ACCURATE_ASSUME_OKAY
}

bool OLBufferDescriptionImpl::IsHandled()
{
    return _mapped ? true : false;
}

error_t OLBufferDescriptionImpl::MapKernel(const OUncontrollableRef<OLGenericMappedBuffer> kernel, pgprot_t prot)
{
    error_t er;
    OLKernelMappedBufferImpl * instance;

    if (!_pages)
        return kErrorOutOfMemory;

    if (_mapped)
        return kErrorAlreadyMapped;

    if (!(instance = new OLKernelMappedBufferImpl()))
        return kErrorOutOfMemory;

    if (ERROR(er = instance->Map(_pages, prot)))
    {
        instance->Destory();
        return er;
    }

    _mapped = instance;
    _user_mapped = false;
    kernel.SetObject(instance);
    
    return kStatusOkay;
}

error_t OLBufferDescriptionImpl::MapUser(const OUncontrollableRef<OLGenericMappedBuffer> kernel, task_k task, pgprot_t prot)
{
    error_t er;
    OLUserMappedBufferImpl * instance;

    if (!_pages)
        return kErrorOutOfMemory;

    if (_mapped)
        return kErrorAlreadyMapped;

    if (!(instance = new OLUserMappedBufferImpl()))
        return kErrorOutOfMemory;

    if (ERROR(er = instance->Map(_pages, task, prot)))
    {
        instance->Destory();
        return er;
    }

    _mapped = instance;
    _user_mapped = true;
    kernel.SetObject(instance);

    return kStatusOkay;
}

void OLBufferDescriptionImpl::SignalUnmapped()
{
    _mapped = nullptr;
    _user_mapped = false; // could be an enum... don't really care... fuck it
}

void OLBufferDescriptionImpl::InvaildateImp()
{
    if (!_mapped)
        _mapped->Destory();
}

OLPageLocation OLMemoryInterfaceImpl::GetPageLocation(size_t max)
{
    // These values are K**i**B 
    // Citiation: https://elixir.bootlin.com/linux/latest/source/arch/x86/mm/init.c#L881

    // Everything above 4GiB is ZONE_NORMAL [can't extract from kernel - no symbol excluding high_memory is exported]
    // Citiation: Truly limited peripherals use memory taken from ZONE_DMA; most of the rest work with ZONE_NORMAL memory. In the 64-bit world, however, things are a little different. There is no need for high memory on such systems, so ZONE_HIGHMEM simply does not exist, and ZONE_NORMAL contains everything above ZONE_DMA. Having (almost) all of main memory contained within ZONE_NORMAL simplifies a lot of things.
    // Citiation: https://lwn.net/Articles/91870/

    if (max > 4llu * 1024llu * 1024llu * 1024llu)
        return kPageNormal;

    if (max > 16 * 1024 * 1024)
        return kPageDMA4GB;
  
    if (max > 0)
        return kPageDMAVeryLow;
    
    return kPageInvalid;
}

phys_addr_t OLMemoryInterfaceImpl::PhysPage(page_k page)
{
    return phys_addr_t(linux_page_to_pfn(page) << kernel_information.LINUX_PAGE_SHIFT);
}

void * OLMemoryInterfaceImpl::MapPage(page_k page)
{
    return (void*)(size_t(PhysPage(page)) + page_offset_base);
}

void  OLMemoryInterfaceImpl::UnmapPage(void * virt)
{
    
}

page_k OLMemoryInterfaceImpl::AllocatePage(OLPageLocation location)
{
    size_t flags;
 
    ASSERT(location != kPageInvalid, "invalid page region");

    flags = 0;
    flags |= GFP_KERNEL;

    switch (location)
    {
    case kPageNormal:
        // if 32 bit, GFP_HIGHUSER
        // ZONE_NORMAL is defacto
        break;
    case kPageDMAVeryLow:
        flags |= GFP_DMA;
        break;
    case kPageDMA4GB:
        flags |= GFP_DMA32;
        break;
    default:
        panic("illegal case statement " __FUNCTION__);
    }
    
    return alloc_pages_current(flags, 0);
}

void OLMemoryInterfaceImpl::FreePage(page_k page)
{
    ASSERT(page, "invalid page " __FUNCTION__);
    __free_pages(page, 0);
}

error_t OLMemoryInterfaceImpl::NewBuilder(const OOutlivableRef<OLBufferDescription> builder)
{
    error_t err;
    OLBufferDescriptionImpl *instance;

    if (!(instance = new OLBufferDescriptionImpl()))
        return kErrorOutOfMemory;

    if (ERROR(err = instance->Construct()))
    {
        instance->Destory();
        return err;
    }

    builder.PassOwnership(instance);
    return kStatusOkay;
}

error_t GetLinuxMemoryInterface(const OUncontrollableRef<OLMemoryInterface> interface)
{
    if (linux_memory)
    {
        interface.SetObject(linux_memory);
        return kStatusOkay;
    }

    linux_memory = interface.SetObject(new OLMemoryInterfaceImpl());
    return linux_memory ? kStatusOkay : kErrorOutOfMemory;
}

void InitMemmory()
{
    linux_memory = nullptr;
    page_offset_base = *(l_unsigned_long*)kallsyms_lookup_name("page_offset_base");
    special_map = zalloc(vm_special_mapping_size());

    ASSERT(NO_ERROR(dyncb_allocate_stub(SYSV_FN(special_map_fault), 4, NULL, &special_map_fault, &special_map_handle)), "couldn't create Xenus memory map area handler");
}