/*
    Purpose: x86_64 linux memory interfaces
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#define DANGEROUS_PAGE_LOGIC
#include <libos.hpp>
#include "OLinuxMemoryMM.hpp"

#define __START_KERNEL_map	_AC(0xffffffff80000000, UL)

static l_unsigned_long page_offset_base = 0;
static uint64_t        phys_base        = 0;
static page_cache_mode cache_mapping[Memory::kCacheMax];

static inline phys_addr_t __phys_addr_nodebug(kernel_pointer_t x)
{
    size_t y = x - __START_KERNEL_map;

    /* use the carry flag to determine if x was < __START_KERNEL_map */
    x = y + ((x > y) ? phys_base : (__START_KERNEL_map - page_offset_base));

    return phys_addr_t(x);
}

pfn_t phys_to_pfn(phys_addr_t address) // NOTE: UP_PAGE COULD BE ILLEGAL (EXCL CONFIG_FLATMEM - WE USE SPARSE MEM!!!!)
{
    pfn_t ret;
    ret.val = size_t(address) >> OS_PAGE_SHIFT;
    return ret;
}

phys_addr_t pfn_to_phys(pfn_t pfn)
{
    return phys_addr_t(size_t(pfn.val) << OS_PAGE_SHIFT);
}

pfn_t page_to_pfn(page_k page)
{
    pfn_t ret;
    ret.val = linux_page_to_pfn(page);
    return ret;
}

page_k pfn_to_page(pfn_t pfn)
{
    return linux_pfn_to_page(pfn.val);
}

pfn_t virt_to_pfn(kernel_pointer_t address)
{
    return phys_to_pfn(__phys_addr_nodebug(address));
}

size_t phys_to_virt(phys_addr_t phys)
{
    return size_t(phys) + page_offset_base;
}

phys_addr_t virt_to_phys(kernel_pointer_t address)
{
    return pfn_to_phys(virt_to_pfn(address));
}

page_k virt_to_page(kernel_pointer_t address)
{
    return pfn_to_page(virt_to_pfn(address));
}

page_cache_mode GetCacheModeFromCacheType(Memory::OLCacheType type)
{
    if (type >= Memory::kCacheMax)
        panicf("Bad protection id %i", type);

    return cache_mapping[type];
}

static void CacheTypeMapping()
{
    cache_mapping[Memory::kCacheCache]          = _PAGE_CACHE_MODE_WB;
    cache_mapping[Memory::kCacheNoCache]        = _PAGE_CACHE_MODE_UC_MINUS;
    cache_mapping[Memory::kCacheWriteCombined]  = _PAGE_CACHE_MODE_WC;
    cache_mapping[Memory::kCacheWriteThrough]   = _PAGE_CACHE_MODE_WT;
    cache_mapping[Memory::kCacheWriteProtected] = _PAGE_CACHE_MODE_WP;
}

void InitMMIOHelper()
{
    phys_base           = *(uint64_t *)       kallsyms_lookup_name("phys_base");
    page_offset_base    = *(l_unsigned_long*) kallsyms_lookup_name("page_offset_base");

    CacheTypeMapping();
}
