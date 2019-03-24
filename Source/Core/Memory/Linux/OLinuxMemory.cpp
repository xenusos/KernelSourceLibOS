/*
    Purpose: OLinuxMemory implementation
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#define DANGEROUS_PAGE_LOGIC
#include <libos.hpp>
#include "OLinuxMemory.hpp"
#if defined(AMD64)
#include "x86_64/OLinuxMemoryVM.hpp"
#include "x86_64/OLinuxMemoryMM.hpp"
#endif

struct LinuxPageEntry 
{
    OLPageEntry entry;
    void * priv;
};

#if defined(AMD64)
static const size_t PAGE_REGION_AMD64_NORMAL_START = 4llu * 1024llu * 1024llu * 1024llu;
static const size_t PAGE_REGION_AMD64_NORMAL_END   = 0xFFFFFFFFFFFFFFFF;

static const size_t PAGE_REGION_AMD64_4GIB_START   = 16 * 1024 * 1024;
static const size_t PAGE_REGION_AMD64_4GIB_END     = (4llu * 1024llu * 1024llu * 1024llu) - 1;

static const size_t PAGE_REGION_AMD64_DMA_START    = 0;
static const size_t PAGE_REGION_AMD64_DMA_END      = (16 * 1024 * 1024) - 1;
#endif

static OLVirtualAddressSpace * memory_kernelspace = nullptr;

OLMemoryInterface *          g_memory_interface = nullptr;

#if defined(AMD64)
static uint16_t * __cachemode2pte_tbl;// [_PAGE_CACHE_MODE_NUM];

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
    entry.prot.pgprot_ &= ~CacheTypeToCacheModeToProt(kCacheCache).pgprot_;
    entry.prot.pgprot_ &= ~CacheTypeToCacheModeToProt(kCacheWriteCombined).pgprot_;
    entry.prot.pgprot_ &= ~CacheTypeToCacheModeToProt(kCacheWriteThrough).pgprot_;
    entry.prot.pgprot_ &= ~CacheTypeToCacheModeToProt(kCacheWriteProtected).pgprot_;
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
    entry.prot.pgprot_ &= ~vm_get_page_prot(VM_WRITE | VM_READ | VM_EXEC).pgprot_;
    entry.prot.pgprot_ |= vm_get_page_prot(vmflags).pgprot_;
    entry.access = access;
}

OLPageEntryMeta OLMemoryInterfaceImpl::CreatePageEntry(size_t access, OLCacheType cache)
{
    OLPageEntryMeta entry = { 0 };
    UpdatePageEntryCache(entry, cache);
    UpdatePageEntryAccess(entry, access);
    return entry;
}

OLPageLocation  OLMemoryInterfaceImpl::GetPageLocation   (size_t max)               
{
#if defined(AMD64)
    // These values are K**i**B 
    // Citiation: https://elixir.bootlin.com/linux/latest/source/arch/x86/mm/init.c#L881

    // Everything above 4GiB is ZONE_NORMAL [can't extract from kernel - no symbol excluding high_memory is exported]
    // Citiation: Truly limited peripherals use memory taken from ZONE_DMA; most of the rest work with ZONE_NORMAL memory. In the 64-bit world, however, things are a little different. There is no need for high memory on such systems, so ZONE_HIGHMEM simply does not exist, and ZONE_NORMAL contains everything above ZONE_DMA. Having (almost) all of main memory contained within ZONE_NORMAL simplifies a lot of things.
    // Citiation: https://lwn.net/Articles/91870/

    if (max >= PAGE_REGION_AMD64_NORMAL_START)
        return kPageNormal;

    if (max >= PAGE_REGION_AMD64_4GIB_START)
        return kPageDMA4GB;
  
//    if (max >= PAGE_REGION_AMD64_DMA_START)
        return kPageDMAVeryLow;
#endif
    return kPageInvalid;
}

size_t OLMemoryInterfaceImpl::GetPageRegionStart(OLPageLocation location)
{
#if defined(AMD64)
    switch (location)
    {
         case kPageDMAVeryLow:
         {
             return PAGE_REGION_AMD64_DMA_START;
         }
         case kPageDMA4GB:
         {
             return PAGE_REGION_AMD64_4GIB_START;
         }
         case kPageNormal:
         {
             return PAGE_REGION_AMD64_NORMAL_START;
         }
         case kPageInvalid:
         default:
         {
             return -1;
         }
    }
#endif
    return -1;
}

size_t OLMemoryInterfaceImpl::GetPageRegionEnd(OLPageLocation location)
{
#if defined(AMD64)
    switch (location)
    {
         case kPageDMAVeryLow:
         {
             return PAGE_REGION_AMD64_DMA_END;
         }
         case kPageDMA4GB:
         {
             return PAGE_REGION_AMD64_4GIB_END;
         }
         case kPageNormal:
         {
             return PAGE_REGION_AMD64_NORMAL_END;
         }
         case kPageInvalid:
         default:
         {
             return -1;
         }
    }
#endif
    return -1;
}

phys_addr_t OLMemoryInterfaceImpl::PhysPage(page_k page)
{
#if defined(AMD64)
    return phys_addr_t(linux_page_to_pfn(page) << kernel_information.LINUX_PAGE_SHIFT);
#else
    return phys_addr_t(-1);
#endif
}

error_t OLMemoryInterfaceImpl::GetKernelAddressSpace(const OUncontrollableRef<OLVirtualAddressSpace> builder)
{
    builder.SetObject(memory_kernelspace);
    return memory_kernelspace == nullptr ? kErrorInternalError : kStatusOkay;
}

error_t OLMemoryInterfaceImpl::GetUserAddressSpace(task_k task, const OOutlivableRef<OLVirtualAddressSpace> builder)
{
#if defined(AMD64)
    if (!builder.PassOwnership(new OLUserVirtualAddressSpaceImpl(task)))
        return kErrorOutOfMemory;
    return kStatusOkay;
#endif
    return kErrorNotImplemented;
}

error_t GetLinuxMemoryInterface(const OUncontrollableRef<OLMemoryInterface> interface)
{
    interface.SetObject(g_memory_interface);
    return g_memory_interface == nullptr ? kErrorInternalError : kStatusOkay;
}


void InitMemmory()
{
#if defined(AMD64)
    InitUserVMMemory();
    InitKernVMMemory();
    InitMMIOHelper();
#endif

    g_memory_interface = new OLMemoryInterfaceImpl();
    ASSERT(g_memory_interface, "couldn't allocate static memory interface");

    memory_kernelspace = new OLKernelVirtualAddressSpaceImpl();
    ASSERT(memory_kernelspace, "couldn't allocate static kernel VM memory interface");

#if defined(AMD64)
    __cachemode2pte_tbl = (uint16_t *)kallsyms_lookup_name("__cachemode2pte_tbl");
    ASSERT(__cachemode2pte_tbl, "couldn't find x86 cache lookup table");
#endif
}
