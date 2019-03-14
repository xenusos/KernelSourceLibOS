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
    //interface.SetObject(nullptr);
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

    //linux_memory = new OLMemoryInterfaceImpl();
    //ASSERT(linux_memory, "couldn't allocate static memory interface");

#if defined(AMD64)
    __cachemode2pte_tbl = (uint16_t *)kallsyms_lookup_name("__cachemode2pte_tbl");
    ASSERT(__cachemode2pte_tbl, "couldn't find x86 cache lookup table");
#endif
}
