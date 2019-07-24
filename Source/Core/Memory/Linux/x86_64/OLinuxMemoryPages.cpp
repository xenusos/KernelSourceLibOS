/*
    Purpose: x86_64 linux memory interfaces
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#define DANGEROUS_PAGE_LOGIC
#include <libos.hpp>
#include "OLinuxMemoryPages.hpp"
#include "OLinuxMemoryMM.hpp"

static int PagesToOrder(int count, int & order)
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


static bool LinuxAllocateContigArray(Memory::PhysAllocationElem * arry, size_t cnt, size_t flags, bool isPfn)
{
    page_k page;
    int order;
    pfn_t pfn;
    size_t total;

    total = PagesToOrder(cnt, order);

    page  = alloc_pages_current(flags | (isPfn ? 0 : __GFP_COMP), order);

    if (!page)
        return false;

    pfn = page_to_pfn(page);
    ASSERT(pfn_to_page(pfn) == page, "Page layout error");

    if (isPfn)
        arry[0].pfn  = pfn;
    else
        arry[0].page = page;

    for (size_t i = 1; i < cnt; i++)
    {
        pfn_t fek;
        fek.val = pfn.val + i;

        if (isPfn)
            arry[i].pfn = pfn;
        else
            arry[i].page = pfn_to_page(fek);
    }

    if (total != cnt)
        LogPrint(kLogVerbose, "A module requested that we give them %i pages, but we gave them %i pages instead. (hint: order of 2) ", cnt, total);

    return true;
}

static void TranslatePageArrayToPFNs(Memory::PhysAllocationElem * arry, size_t cnt)
{
    for (size_t i = 0; i < cnt; i++)
    {
        arry[i].pfn = page_to_pfn(arry[i].page);
    }
}

static bool LinuxAllocatePages(Memory::PhysAllocationElem * arry, size_t cnt, size_t flags, bool isPfn)
{
    page_k page;

    for (size_t i = 0; i < cnt; i++)
    {
        page = alloc_pages_current(flags, 0);
        
        if (!page)
            goto error;

        arry[i].page = page;
    }

    if (isPfn)
        TranslatePageArrayToPFNs(arry, cnt);

    return true;

error:
    for (size_t i = 0; i < cnt; i++)
    {
        page = arry[i].page;

        if (!page) 
            continue;

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
            size_t byPfn  : 1;
        };
        union
        {
            size_t integer;
            page_k ptr;
        } val;
    };
};

#define PAGE_ARRAY_POINTER_MAGIC 0xAABBCC11

#pragma pack(pop)


Memory::PhysAllocationElem * AllocateLinuxPages(Memory::OLPageLocation location, size_t cnt, bool user, bool contig, bool byPfn, size_t uflags)
{
    size_t flags;
    Memory::PhysAllocationElem * arry;
    EncodedArrayMeta meta;
    bool ret;

    ASSERT(location != Memory::kPageInvalid, "invalid page region");

    arry = reinterpret_cast<Memory::PhysAllocationElem *>(calloc(cnt + 2, sizeof(Memory::PhysAllocationElem)));
    
    if (!arry)
        return nullptr;

    (arry++)->magic = PAGE_ARRAY_POINTER_MAGIC;

    // start the array with an entry that contains metadata instead of a pointer
    meta.contig = contig;
    meta.length = cnt;
    meta.byPfn  = byPfn;

    (arry++)->page = meta.val.ptr;

    // Xenus to Linux flag translation
    flags = 0;

    if (user)
        flags |= GFP_USER;
    else
        flags |= GFP_KERNEL /*user differs with the addition of __GFP_HARDWALL. afaik this does NOT matter*/;

    if (uflags & Memory::OL_PAGE_ZERO)
        flags |= __GFP_ZERO;

    switch (location)
    {
    case Memory::kPageNormal:
        // if 32 bit, GFP_HIGHUSER
        // ZONE_NORMAL is defacto
        break;
    case Memory::kPageDMAVeryLow:
        flags |= GFP_DMA;
        break;
    case Memory::kPageDMA4GB:
        flags |= GFP_DMA32;
        break;
    default:
        panic("illegal case statement " __FUNCTION__);
    }

    if (contig)
        ret = LinuxAllocateContigArray(arry, cnt, flags, byPfn);
    else
        ret = LinuxAllocatePages(arry, cnt, flags, byPfn);
  
    if (!ret)
    {
        free(&arry[-2]);
        return nullptr;
    }

    return arry;
}

void FreeLinuxPages(Memory::PhysAllocationElem * pages)
{
    int order;
    EncodedArrayMeta meta;

    ASSERT(pages, "invalid parameter");

    meta.val.ptr = pages[-1].page;

    ASSERT(pages[-2].magic == PAGE_ARRAY_POINTER_MAGIC, "A page array was given to FreeLinuxPages; however, we didn't provide this pointer. Prefixed data has potentially been lost.")

    if (meta.contig)
    {
        page_k page = meta.byPfn ? pfn_to_page(pages[0].pfn) : pages[0].page;

        PagesToOrder(meta.length, order);
        __free_pages(page, order);
    }
    else
    {
        for (size_t i = 0; i < meta.length; i++)
        {
            __free_pages(meta.byPfn ? pfn_to_page(pages[i].pfn) : pages[i].page, 0);
        }
    }

    free(&pages[-2]);
}
