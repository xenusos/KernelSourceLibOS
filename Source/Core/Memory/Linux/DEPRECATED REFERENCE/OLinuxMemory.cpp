/*
    Purpose: x86_64 linux memory interfaces
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#define DANGEROUS_PAGE_LOGIC
#include <libos.hpp>
#include "OLinuxMemory.hpp"

static l_unsigned_long page_offset_base = 0;
static OLMemoryInterface * linux_memory = 0;

#include "../../Processes/OProcesses.hpp"
#define MEMORY_DEVICE "XenusMemoryMapper"

static void * special_map_fault;
static void * special_map_handle;
static vm_special_mapping_k special_map;

DEFINE_SYSV_FUNCTON_START(special_map_fault, size_t)
const vm_special_mapping_k sm,
vm_area_struct_k vma,
vm_fault_k vmf,
void * pad,
DEFINE_SYSV_FUNCTON_END_DEF(special_map_fault, size_t)
{
    LogPrint(LoggingLevel_e::kLogError, "something bad happened. fault at @ %p in task_struct %p", vm_fault_get_address_size_t(vmf), OSThread);
    SYSV_FUNCTON_RETURN(0)
}
DEFINE_SYSV_END

OLKernelMappedBufferImpl::OLKernelMappedBufferImpl()
{
    _length = 0;
    _vm     = nullptr;
    _va     = nullptr;
    _mapped = false;
}

error_t OLKernelMappedBufferImpl::GetVAStart(size_t& start)
{
    CHK_DEAD;

    if (!_va)
        return kErrorNotMapped;

    if (!_mapped)
        return kErrorNotMapped;

    start = size_t(_va);
    return kStatusOkay;
}

error_t OLKernelMappedBufferImpl::GetVAEnd(size_t& end)
{
    CHK_DEAD;
    
    if (!_va)
        return kErrorNotMapped;
    
    if (!_mapped)
        return kErrorNotMapped;
    
    end = size_t(_va) + _length;
    return kStatusOkay;
}

error_t OLKernelMappedBufferImpl::GetLength(size_t& length)
{
    CHK_DEAD;
    length = _length;
    return kStatusOkay;
}

error_t OLKernelMappedBufferImpl::Unmap()
{
    CHK_DEAD;
    if (_va)
    {
        // deallocates _vm
        vunmap(_va);
    }
    _va     = 0;
    _vm     = nullptr;
    _mapped = false;
    return kStatusOkay;
}

error_t OLKernelMappedBufferImpl::CreateAddress(size_t pages, size_t & out)
{
    CHK_DEAD;

    _vm = __get_vm_area(pages << OS_PAGE_SHIFT, 0, kernel_information.LINUX_VMALLOC_START, kernel_information.LINUX_VMALLOC_END);
    
    if (!_vm)
    {
        Invalidate();
        return kErrorInternalError;
    }

    _va     = _vm->addr;
    _length = PAGE_SIZE * pages;

    out     = (size_t)_va;
    return kStatusOkay;
}

error_t OLKernelMappedBufferImpl::Remap(dyn_list_head_p pages, size_t count, OLPageEntryMeta prot)
{
    CHK_DEAD;
    page_k *entry;
    error_t ret;
    l_int l;

    if (_mapped)
    {
        if (ERROR(ret = Unmap()))
            return ret;
    }

    if (ERROR(ret = dyn_list_get_by_index(pages, 0, (void **)&entry)))
        return ret;

    for (size_t i = 0; i < count; i++)
    {
        if (!entry[i])
        {
            LogPrint(LoggingLevel_e::kLogError, "Found NULL page entry in descriptor, idx %lli. You can't do this bullshit in the kernel; usermode is fine & will cause an internal trap, kernel would crash", i);
            return kErrorGenericFailure;
        }
    }

    l = map_vm_area(_vm, prot.prot, entry);
    
    if (l)
        return kErrorInternalError;

    _mapped = true;

    return kStatusOkay;
}

void OLKernelMappedBufferImpl::InvalidateImp()
{
    Unmap();
}

OLUserMappedBufferImpl::OLUserMappedBufferImpl(task_k task)
{
    _length = 0;
    _va     = 0;
    _mapped = false;
    _task   = task;
    _no_unmap = false;
    ProcessesTaskIncrementCounter(task);
}

error_t OLUserMappedBufferImpl::GetVAStart(size_t& end)
{
    CHK_DEAD;

    if (!_va)
        return kErrorNotMapped;

    if (!_mapped)
        return kErrorNotMapped;

    end =  _va;
    return kStatusOkay;
}

error_t OLUserMappedBufferImpl::GetVAEnd(size_t& end)
{
    CHK_DEAD;

    if (!_va)
        return kErrorNotMapped;

    if (!_mapped)
        return kErrorNotMapped;

    end = _va + _length;
    return kStatusOkay;
}

error_t OLUserMappedBufferImpl::GetLength(size_t& length)
{
    CHK_DEAD;
    length = _length;
    return kStatusOkay;
}

error_t OLUserMappedBufferImpl::CreateAddress(size_t pages, size_t & out)
{
    CHK_DEAD;
    size_t map;
    error_t ret;
    mm_struct_k mm;
    vm_area_struct_k area;

    ret = kStatusOkay;

    mm = ProcessesAcquireMM_Write(_task);
    if (!mm)
        return kErrorInternalError;

    map = (size_t)get_unmapped_area(NULL, 0, pages << OS_PAGE_SHIFT, 0, 0);  // find an unused area in the processes vmarea

    if (!map)
    {
        Invalidate();
        ret = kErrorInternalError; goto out;
    }

    // abuse a static kernel function to create VMAs 
    area = _install_special_mapping(mm, (l_unsigned_long)map, pages << OS_PAGE_SHIFT,  VM_MAYWRITE | VM_MAYREAD | VM_MAYEXEC | VM_SHARED, special_map); 
    if (!area)
    {
        Invalidate();
        ret = kErrorInternalError; goto out;
    }

    out     = map;
    _area   = area;
    _va     = map;
    _length = PAGE_SIZE * pages;

out:
    ProcessesReleaseMM_Write(mm);
    return ret;
}

error_t OLUserMappedBufferImpl::Remap(dyn_list_head_p pages, size_t count, OLPageEntryMeta prot)
{
    CHK_DEAD;
    error_t ret;
    page_k *entry;
    mm_struct_k mm;

    ret = kStatusOkay;

    if (_mapped)
    {
        if (ERROR(ret = Unmap()))
            return ret;
    }

    if (ERROR(ret = dyn_list_get_by_index(pages, 0, (void **)&entry)))
        return ret;

    for (size_t i = 0; i < count; i++)
    {
        if (!entry[i])
        {
            LogPrint(LoggingLevel_e::kLogError, "Found NULL page entry in descriptor, idx %lli", i);
            return kErrorGenericFailure;
        }
    }

    mm = ProcessesAcquireMM_Write(_task);
    if (!mm)
        return kErrorInternalError;

    // update page protection before commit
    {
        vm_area_struct_set_vm_page_prot_uint64(_area, prot.prot.pgprot_);
    }

    // update vm flags
    {
        l_unsigned_long flags;

        flags = mm_struct_get_def_flags_size_t(mm);
        flags |= VM_DONTEXPAND | VM_SOFTDIRTY;
        flags |= VM_MAYWRITE | VM_MAYREAD | VM_MAYEXEC | VM_SHARED;
        
        if (prot.access & OL_ACCESS_READ)
            flags |= VM_READ;
        
        if (prot.access & OL_ACCESS_WRITE)
        {
            flags |= VM_READ;
            flags |= VM_WRITE;
        }

        if (prot.access & OL_ACCESS_EXECUTE)
        {
            flags |= VM_READ;
            flags |= VM_EXEC;
        }

        vm_area_struct_set_vm_flags_size_t(_area, flags);
    }

    // ram our massive pages into the general vm area 
    {
        // insert pages into the PTE
        for (size_t i = 0; i < count; i++)
        {
            l_int error;

            if (!entry[i])
                continue;

            // try insert; must be post pgprot update
            error = vm_insert_page(_area, (l_unsigned_long)_va + (i << OS_PAGE_SHIFT), entry[i]);

            ASSERT(error == 0, "fatal error occurred while inserting user page");
        }
    }

    // free semaphore
    ProcessesReleaseMM_Write(mm);

    _mapped = true;
    return kStatusOkay;
}

error_t OLUserMappedBufferImpl::Unmap()
{
    error_t ret;
    
    ret = kStatusOkay;

    if (_va && _task)
    {
        mm_struct_k mm;

        mm = ProcessesAcquireMM(_task);

        if (!mm)
            return kErrorInternalError;

        //vm_munmap_ex -> vm_munmap -> remove_vma_list -> remove_vma kmem_cache_free(vm_area_cachep, vma);
        vm_munmap_ex(mm, _va, _length);

        ProcessesReleaseMM_NoLock(mm);
    }

    _va     = 0;
    _length = 0;
    _area   = nullptr;
    _mapped = false;
    return kStatusOkay;
}

void  OLUserMappedBufferImpl::DisableUnmapOnFree()
{
    _no_unmap = true;

    if (_task)
    {
        ProcessesTaskDecrementCounter(_task);
        _task = nullptr;
    }
}

void OLUserMappedBufferImpl::InvalidateImp()
{
    if (_no_unmap)
        return;

    Unmap();   
    
    if (_task)
    {
        ProcessesTaskDecrementCounter(_task);
        _task = nullptr;
    }
}

OLBufferDescriptionImpl::OLBufferDescriptionImpl(dyn_list_head_p pages)
{
    _pages         = pages;
    _mapped_user   = nullptr;
    _mapped_kernel = nullptr;
    _cnt           = 0;
}

bool    OLBufferDescriptionImpl::PageIsPresent(size_t idx)
{
    void * buf;

    if (ERROR(dyn_list_get_by_index(_pages, idx, &buf)))
        return false;

    return true;
}

error_t OLBufferDescriptionImpl::PageInsert(size_t idx, page_k page)
{
    page_k *entry;
    size_t cnt;
    error_t ret;

    if (ERROR(ret = dyn_list_entries(_pages, &cnt)))
        return ret;

    if (cnt == idx)
    {
        // append
        if (_mapped_kernel)
            return kErrorAlreadyMapped;

        if (_mapped_user)
            return kErrorAlreadyMapped;

        if (ERROR(ret = dyn_list_append(_pages, (void **)&entry)))
            return ret;

        _cnt++;
    }
    else
    {
        // update
        if (ERROR(ret = dyn_list_get_by_index(_pages, idx, (void **)&entry)))
            return ret;
    }

    *entry = page;
    return kStatusOkay;
}

error_t OLBufferDescriptionImpl::PageCount(size_t & out)
{
    page_k *entry;
    size_t cnt;
    error_t ret;

    if (ERROR(ret = dyn_list_entries(_pages, &cnt)))
        return ret;

    out = cnt;
    return kStatusOkay;
}

error_t OLBufferDescriptionImpl::PagePhysAddr(size_t idx, phys_addr_t & addr)
{
    error_t err;
    page_k *entry;

    if (ERROR(dyn_list_get_by_index(_pages, idx, (void **)&entry)))
        return false;

    if (!(*entry))
        return kErrorPageNotFound;

    addr = linux_memory->PhysPage(*entry);
    return addr ? kStatusOkay : XENUS_ERROR_OS_UNSUPPORTED;
}

error_t OLBufferDescriptionImpl::PageMap(size_t idx, void * & addr)
{
    error_t err;
    page_k *entry;

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

error_t OLBufferDescriptionImpl::SetupUserAddress(task_k task, size_t & out)
{
    error_t er;
    OLUserMappedBufferImpl * instance;

    if (_mapped_user)
    {
        if (_mapped_user->IsDead())
        {
            _mapped_user->Destory();
            _mapped_user = nullptr;
        }
        else
        {
            return kErrorAlreadyMapped;
        }
    }

    if (!(instance = new OLUserMappedBufferImpl(task)))
        return kErrorOutOfMemory;

    if (ERROR(er = instance->CreateAddress(_cnt, out)))
    {
        instance->Destory();
        return er;
    }

    _mapped_user = instance;
    return kStatusOkay;
}

error_t OLBufferDescriptionImpl::SetupKernelAddress(size_t & out)
{
    error_t er;
    OLKernelMappedBufferImpl * instance;

    if (_mapped_kernel)
    {
        if (_mapped_kernel->IsDead())
        {
            _mapped_kernel->Destory();
            _mapped_kernel = nullptr;
        }
        else
        {
            return kErrorAlreadyMapped;
        }
    }

    if (!(instance = new OLKernelMappedBufferImpl()))
        return kErrorOutOfMemory;

    if (ERROR(er = instance->CreateAddress(_cnt, out)))
    {
        instance->Destory();
        return er;
    }

    _mapped_kernel = instance;
    return kStatusOkay;
}

error_t OLBufferDescriptionImpl::MapUser(const OUncontrollableRef<OLGenericMappedBuffer> kernel, OLPageEntryMeta prot)
{
    error_t er;

    if (!_mapped_user)
        return kErrorNotMapped;

    if (ERROR(er = _mapped_user->Remap(_pages, _cnt, prot)))
        return er;

    kernel.SetObject(_mapped_user);
    return kStatusOkay;
}

error_t OLBufferDescriptionImpl::MapKernel(const OUncontrollableRef<OLGenericMappedBuffer> kernel, OLPageEntryMeta prot)
{
    error_t er;

    if (!_mapped_kernel)
        return kErrorNotMapped;

    if (ERROR(er = _mapped_kernel->Remap(_pages, _cnt, prot)))
        return er;

    kernel.SetObject(_mapped_kernel);
    return kStatusOkay;
}

error_t OLBufferDescriptionImpl::UpdateUser(OLPageEntryMeta prot)
{
    error_t er;

    if (!_mapped_user)
        return kStatusOkay;

    if (ERROR(er = _mapped_user->Remap(_pages, _cnt, prot)))
        return er;

    return kStatusOkay;
}

error_t OLBufferDescriptionImpl::UpdateKernel(OLPageEntryMeta prot)
{
    error_t er;

    if (!_mapped_kernel)
        return kStatusOkay;

    if (ERROR(er = _mapped_kernel->Remap(_pages, _cnt, prot)))
        return er;

    return kStatusOkay;
}

error_t OLBufferDescriptionImpl::UpdateAll(OLPageEntryMeta prot)
{
    error_t er;
    
    if (ERROR(er = UpdateUser(prot)))
        return er;

    if (ERROR(er = UpdateKernel(prot)))
        return er;

    return kStatusOkay;
}

void OLBufferDescriptionImpl::InvalidateImp()
{
    if (_mapped_kernel)
        _mapped_kernel->Destory();
    if (_mapped_user)
        _mapped_user->Destory();
}

OLPageLocation OLMemoryInterfaceImpl::GetPageLocation(size_t max)
{
#if defined(AMD64)
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
#endif
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

page_k OLMemoryInterfaceImpl::AllocatePage(OLPageLocation location, size_t uflags)
{
    size_t flags;
    page_k page;
 
    ASSERT(location != kPageInvalid, "invalid page region");

    flags = 0;
    flags |= GFP_KERNEL /*user differs with the addition of __GFP_HARDWALL. afaik we dont need such*/;

    if (uflags & OL_PAGE_ZERO)
        flags |= __GFP_ZERO;

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

#if defined(AMD64)
uint16_t * __cachemode2pte_tbl;// [_PAGE_CACHE_MODE_NUM];

static inline unsigned long cachemode2protval(enum page_cache_mode pcm)
{
    if (pcm == 0)
        return 0;
    return __cachemode2pte_tbl[pcm];
}
#endif

static pgprot_t CacheTypeToCacheModeToProt(OLCacheType cache)
{
#if defined(AMD64)
    pgprot_t prot;
    prot.pgprot_ = 0;

    switch (cache)
    {
    case kCacheCache:
    {
        // _PAGE_CACHE_MODE_WB = no op
        return prot;
    }
    case kCacheNoCache:
    {
        prot.pgprot_ = cachemode2protval(_PAGE_CACHE_MODE_UC);
        return prot;
    }
    case kCacheWriteCombined:
    {
        prot.pgprot_ = cachemode2protval(_PAGE_CACHE_MODE_WC);
        return prot;
    }
    case kCacheWriteThrough:
    {
        prot.pgprot_ = cachemode2protval(_PAGE_CACHE_MODE_WT);
        return prot;
    }
    case kCacheWriteProtected:
    {
        prot.pgprot_ = cachemode2protval(_PAGE_CACHE_MODE_WP);
        return prot;
    }
    default:
    {
        panicf("Bad protection id %i", cache);
    }
    }
#else
pgprot_noncached(vm_page_prot)
pgprot_writecombine(vm_page_prot)
pgprot_dmacoherent(vm_page_prot)
#endif
    return prot;
}

void OLMemoryInterfaceImpl::UpdatePageEntryCache(OLPageEntryMeta &entry, OLCacheType cache)
{
    entry.prot.pgprot_ |= CacheTypeToCacheModeToProt(cache).pgprot_;
}

void OLMemoryInterfaceImpl::UpdatePageEntryAccess(OLPageEntryMeta &entry, size_t access)
{
    size_t vmflags;
    vmflags = 0;

    if (access & OL_ACCESS_EXECUTE)
        vmflags |= VM_EXEC;

    if (access & OL_ACCESS_READ)
        vmflags |= VM_READ;

    if (access & OL_ACCESS_WRITE)
        vmflags |= VM_WRITE;

    vmflags |= VM_SHARED; // _PAGE_RW is required even when not writing.
                          // udmabuf also uses this logic alongside similar x86 cache logic
    entry.prot.pgprot_ |= vm_get_page_prot(vmflags).pgprot_;
    entry.access = access;
}

OLPageEntryMeta OLMemoryInterfaceImpl::CreatePageEntry(size_t access, OLCacheType cache)
{
    OLPageEntryMeta entry = { 0 };
    UpdatePageEntryAccess(entry, access);
    UpdatePageEntryCache(entry, cache);
    return entry;
}

error_t OLMemoryInterfaceImpl::NewBuilder(const OOutlivableRef<OLBufferDescription> builder)
{
    dyn_list_head_p pages;

    pages = DYN_LIST_CREATE(page_k);

    if (!pages)
        return kErrorOutOfMemory;

    if (!builder.PassOwnership(new OLBufferDescriptionImpl(pages)))
    {
        dyn_list_destory(pages);
        return kErrorOutOfMemory;
    }

    return kStatusOkay;
}

error_t GetLinuxMemoryInterface(const OUncontrollableRef<OLMemoryInterface> interface)
{
    interface.SetObject(linux_memory);
    return kStatusOkay;
}

void InitMemorySpecialMap()
{
    error_t err;

    special_map = zalloc(vm_special_mapping_size());
    ASSERT(special_map, "allocate special mapping handler");

    err = dyncb_allocate_stub(SYSV_FN(special_map_fault), 4, NULL, &special_map_fault, &special_map_handle);
    ASSERT(NO_ERROR(err), "couldn't create Xenus memory map fault handler");

    vm_special_mapping_set_fault_size_t(special_map, size_t(special_map_fault));
    vm_special_mapping_set_name_size_t(special_map,  size_t("XenusMemoryArea"));
}

void InitMemmory()
{
    InitMemorySpecialMap();

    page_offset_base = *(l_unsigned_long*)kallsyms_lookup_name("page_offset_base");
    ASSERT(special_map, "couldn't allocate special mapping struct");

    linux_memory = new OLMemoryInterfaceImpl();
    ASSERT(linux_memory, "couldn't allocate static memory interface");

#if defined(AMD64)
    __cachemode2pte_tbl = (uint16_t *)kallsyms_lookup_name("__cachemode2pte_tbl");
    ASSERT(__cachemode2pte_tbl, "couldn't find x86 cache lookup table");
#endif
}
