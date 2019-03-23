/*
    Purpose: x86_64 linux memory interfaces
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#define DANGEROUS_PAGE_LOGIC
#include <libos.hpp>
#include "OLinuxMemory.hpp"
#include "OLinuxMemoryMM.hpp"

#define __START_KERNEL_map	_AC(0xffffffff80000000, UL)

static l_unsigned_long page_offset_base = 0;
static uint64_t        phys_base        = 0;

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

static int pagesToOrder(int count, int & order)
{
    if (count == 1)
    {
        order = 0;
        return 1;
    }

    for (int i = 0; i < 31; i++)
    {
        int pages = (1 << i);
        if (pages > count)
        {
            order = i;
            return pages;
        }
    }

    panicf("Couldn't find order for %i pages", count);
    return 0;
}
 
static bool LinuxAllocateContigArray(page_k * arry, size_t cnt, size_t flags)
{
    page_k page;
    int order;
    pfn_t pfn;
    size_t total;

    total = pagesToOrder(cnt, order);

    page  = alloc_pages_current(flags | __GFP_COMP, order);
    if (!page)
        return false;

    arry[0] = page;

    pfn = page_to_pfn(page);
    ASSERT(pfn_to_page(pfn) == page, "Page layout error");

    for (int i = 1; i < cnt; i++)
    {
        pfn_t fek;
        fek.val = pfn.val + i;
        arry[i] = pfn_to_page(fek);
    }

    if (total != cnt)
        LogPrint(kLogVerbose, "A module requested that we give them %i pages, but we gave them %i pages instead. (hint: order of 2) ", cnt, total);

    return true;
}

static bool LinuxAllocatePages(page_k * arry, size_t cnt, size_t flags)
{
    for (size_t i = 0; i < cnt; i++)
    {
        page_k page;

        page = alloc_pages_current(flags, 0);

        if (!page)
            goto error;

        arry[i] = page;
    }

    return true;

error:
    for (size_t i = 0; i < cnt; i++)
    {
        page_k page = arry[i];

        if (!page) continue;

        __free_pages(page, 0);
    }

    return false;
}

#pragma pack(push, 1)
struct EncodedArrayMeta // Why do bitwise hackery when the compiler can do it for us :DD
{
    union
    {
        struct
        {
            size_t length : 24;
            size_t contig : 1;
        };
        union
        {
            size_t integer;
            page_k ptr;
        } val;
    };
};
#pragma pack(pop)

page_k * AllocateLinuxPages(OLPageLocation location, size_t cnt, bool user, bool contig, size_t uflags)
{
    size_t flags;
    page_k * arry;
    EncodedArrayMeta meta;
    bool ret;

    ASSERT(location != kPageInvalid, "invalid page region");

    arry = (page_k *)calloc(cnt + 1, sizeof(page_k));
    
    if (!arry)
        return nullptr;

    // start the array with an entry that contains metadata instead of a pointer
    meta.contig = contig;
    meta.length = cnt;

    *(arry++) = meta.val.ptr;

    // Xenus to Linux flag translation
    flags = 0;

    if (user)
        flags |= GFP_USER;
    else
        flags |= GFP_KERNEL /*user differs with the addition of __GFP_HARDWALL. afaik this does NOT matter*/;

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

    if (contig)
        ret = LinuxAllocateContigArray(arry, cnt, flags);
    else
        ret = LinuxAllocatePages(arry, cnt, flags);
  
    if (!ret)
    {
        free(&arry[-1]);
        return nullptr;
    }

    return arry;
}

void FreeLinuxPages(page_k * pages)
{
    int order;
    EncodedArrayMeta meta;

    ASSERT(pages, "invalid parameter");

    meta.val.ptr = pages[-1];

    if (meta.contig)
    {
        pagesToOrder(meta.length, order);

        __free_pages(pages[0], order);
    }
    else
    {
        for (size_t i = 0; i < meta.length; i++)
        {
            __free_pages(pages[i], 0);
        }
    }

    free(&pages[-1]);
}

void InitMMIOHelper()
{
    phys_base        = *(uint64_t *)       kallsyms_lookup_name("phys_base");
    page_offset_base = *(l_unsigned_long*) kallsyms_lookup_name("page_offset_base");
}
