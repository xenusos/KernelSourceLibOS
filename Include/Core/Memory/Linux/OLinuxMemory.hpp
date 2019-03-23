/*
    Purpose: Linux specific low-level memory operations
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#pragma once

#include <ITypes/IVMFault.hpp>

const size_t OL_ACCESS_READ         = (1 << 0);
const size_t OL_ACCESS_WRITE        = (1 << 1);
const size_t OL_ACCESS_EXECUTE      = (1 << 2);

const size_t OL_PAGE_ZERO           = (1 << 0);

enum OLCacheType
{
    kCacheCache          = 0,
    kCacheWriteCombined  = 1,
    kCacheNoCache        = 2,
    kCacheWriteThrough   = 3, // read cache, write cache and physical
    kCacheWriteProtected = 4  // read cache, write physical
};
// PPC and X86 have a lot more cache types
// driver devs just dont need to use them

enum OLPageLocation
{
    kPageInvalid = -1,
    kPageDMAVeryLow, // ZONE_DMA     < 16M
    kPageDMA4GB,     // ZONE_DMA32   < 2 ^ 32 -1
    kPageNormal      // ZONE_NORMAL  
};

struct OLPageEntryMeta
{
    pgprot_t prot;
    size_t access;
    OLCacheType cache;
};

enum OLPageEntryType
{
    kPageEntryByAddress,
    kPageEntryByPage,
    kPageEntryDummy
};

struct OLPageEntry
{
    OLPageEntryMeta meta;
    OLPageEntryType type;
    union
    {
        phys_addr_t address;
        page_k page;
    };
};

class OLMemoryAllocation;
typedef l_int(* OLTrapHandler_f)(OPtr<OLMemoryAllocation> space, size_t address, IVMFault & fault, void * context);
// HINT: __do_fault
// HINT: handle_pte_fault
// HINT: https://lwn.net/Articles/242625/
// TRY: VM_FAULT_OKAY, VM_FAULT_ERROR, VM_FAULT_NOPAGE, VM_FAULT_RETRY, VM_FAULT_DONE_COW

// Describes a virtual buffer [or two if shared between user and kernel] pre-PTE allocation
class OLMemoryAllocation : public OObject
{
public:
    // Important notes:
    //  The following functions are O(N) NOT O(log(n)) or better - relative to injected pages, not ::SizeInPages() 
    //  ~~REDACTED YOU CAN NOW INSERT DUMMY PAGES AND PHYS REGIONS INTO THE KERNEL VM~~ 
    virtual bool    PageIsPresent (size_t idx)                                                                           = 0;
    virtual error_t PageInsert    (size_t idx, OLPageEntry page)                                                         = 0;
    virtual error_t PagePhysAddr  (size_t idx, phys_addr_t & addr)                                                       = 0;
    virtual error_t PageGetMapping(size_t idx, OLPageEntry & page)                                                       = 0;
                                                                                                                         
    virtual size_t  SizeInPages   ()                                                                                     = 0;
    virtual size_t  SizeInBytes   ()                                                                                     = 0;
                                                                                                                            
    virtual size_t  GetStart      ()                                                                                     = 0;
    virtual size_t  GetEnd        ()                                                                                     = 0;
    
    virtual void    SetTrapHandler(OLTrapHandler_f cb, void * data)                                                      = 0;

    virtual void    ForceLinger   ()                                                                                     = 0;
};

class OLVirtualAddressSpace : public OObject
{
public:

    virtual page_k * AllocatePages(OLPageLocation location, size_t cnt, bool contig, size_t flags = 0)                   = 0;
    virtual void     FreePages    (page_k * pages)                                                                       = 0;
                                                                                                                         
    virtual error_t  MapPhys      (phys_addr_t phys, size_t pages, size_t & address, void * & context)                   = 0;
    virtual error_t  UnmapPhys    (void * context)                                                                       = 0;
                                                                                                                         
    virtual error_t  MapPage      (page_k page, size_t & address, void * & context)                                      = 0;
    virtual error_t  UnmapPage    (void * context)                                                                       = 0;
                                                                                                                         
    virtual error_t  NewDescriptor(size_t start, size_t pages, const OOutlivableRef<OLMemoryAllocation> allocation)      = 0;
};

class OLMemoryInterface : public OObject
{
public:

    virtual OLPageLocation  GetPageLocation      (size_t max)                                                            = 0;
    virtual size_t          GetPageRegionStart   (OLPageLocation location)                                               = 0; 
    virtual size_t          GetPageRegionEnd     (OLPageLocation location)                                               = 0;
                                                                                                                         
    virtual phys_addr_t     PhysPage             (page_k page)                                                           = 0;
                                                                                                                          
    virtual void            UpdatePageEntryCache (OLPageEntryMeta &, OLCacheType cache)                                  = 0;
    virtual void            UpdatePageEntryAccess(OLPageEntryMeta &, size_t access)                                      = 0;
    virtual OLPageEntryMeta CreatePageEntry      (size_t access, OLCacheType cache)                                      = 0;
                                                                                                                         
    virtual error_t         GetKernelAddressSpace(const OUncontrollableRef<OLVirtualAddressSpace> builder)               = 0;
    virtual error_t         GetUserAddressSpace  (task_k task, const OOutlivableRef<OLVirtualAddressSpace> builder)      = 0;
};

// about dma:
//  on x86, all we need is a memory fence to ensure that we're synced
//  on linux builds targeting x86[_64/_32], dma_ops point to nommu_dma_ops (assuming AMD gart hackery isn't used which uses similar logic)
//  nommu_dma_ops.sync_??_for_device = flush_write_buffers() { barrier }
// 
//  MSDN docs just state "idk man- PAGED BUFFERS" without actually defining what makes dma buffers from a dma device special
//  the dxgk subsystem just allocates generic NC PTEs given an MDL 

LIBLINUX_SYM error_t GetLinuxMemoryInterface(const OUncontrollableRef<OLMemoryInterface> interface);
