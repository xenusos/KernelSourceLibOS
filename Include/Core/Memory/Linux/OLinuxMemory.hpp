/*
    Purpose: Linux specific low-level memory operations
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#pragma once

// Describes mapped memory
class OLGenericMappedBuffer : public OObject
{
public:
    virtual error_t GetVAStart(size_t &)                            = 0;
    virtual error_t GetVAEnd(size_t &)                              = 0;
    virtual error_t GetLength(size_t &)                             = 0;
    virtual error_t Unmap()                                         = 0;
};

// Describes a virtual buffer [or two if shared between user and kernel] pre-PTE allocation
class OLBufferDescription : public OObject
{
public:
    
    // Add/remove/modify pages in virtual buffer
    virtual bool    PageIsPresent(size_t idx)                       = 0;
    virtual error_t PageInsert   (size_t idx, page_k page)          = 0;
    virtual error_t PagePhysAddr (size_t idx, phys_addr_t & addr)   = 0;
    virtual error_t PageCount    (size_t & count)                   = 0;

    // Temporarily access pages within this descriptior
    virtual error_t PageMap      (size_t idx, void * & addr)        = 0;
    virtual void    PageUnmap    (void * addr)                      = 0;

    // Map
    virtual error_t MapKernel(const OUncontrollableRef<OLGenericMappedBuffer> kernel, pgprot_t prot)              = 0; // OUncontrollableRef -> life is controlled by OLBufferDescriptions container [or lack thereof]
    virtual error_t MapUser  (const OUncontrollableRef<OLGenericMappedBuffer> kernel, task_k task, pgprot_t prot) = 0;

    // Remap
    virtual error_t UpdateMaps(pgprot_t prot)                       = 0;
};

enum OLPageLocation
{
    kPageInvalid = -1,
    kPageDMAVeryLow, // ZONE_DMA     < 16M
    kPageDMA4GB,     // ZONE_DMA32   < 2 ^ 32 -1
    kPageNormal      // ZONE_NORMAL  
};

class OLMemoryInterface
{
public:
    virtual OLPageLocation GetPageLocation(size_t max)                            = 0;           // nvidya demands ranges of (0, (1 << adapter bits) - 1) 
                                                                                                 // lets be nice to them
    virtual page_k AllocatePage(OLPageLocation location)                          = 0;
    virtual void   FreePage(page_k page)                                          = 0;

    virtual phys_addr_t   PhysPage(page_k page)                                   = 0;
    virtual void *         MapPage(page_k page)                                   = 0;
    virtual void         UnmapPage(void * virt)                                   = 0;

    virtual error_t NewBuilder(const OOutlivableRef<OLBufferDescription> builder) = 0;

    //virtual error_t AllocateDMA(const OOutlivableRef<OLDmaBuffer> dma, size_t length) = 0;
    //virtual error_t AllocateDMA(const OOutlivableRef<OLDmaBuffer> dma, page_k page)   = 0;

};

// about dma:
//  on x86, all we need is a memory fence to ensure that we're synced
//  dma_ops on linux builds targeting x86_64 just point to nommu_dma_ops [assuming amd gart hackery isn't used]
//  nommu_dma_ops.sync_??_for_device = flush_write_buffers();
// 
//  MSDN docs just state "idk man- PAGED BUFFERS" without actually defining what makes dma buffers from a dma device special
//  the dxgk subsystem just allocates generic no-cache PTEs to an MDL 
//
//  we could implement an OLDmaBuffer to be safe, but since 90% of Xenus is x86_64 only, there is no point
//class OLDmaBuffer
//{
//public:
//    virtual error_t GetVirtualAddress(void * & mapped)        = 0;
//    virtual error_t GetPhysicalAddress(dma_addr_t & addr)     = 0;
//                                                              
//    // DMA sync stubs to be used alongside GetVirtualAddress  
//    virtual void PreWrite()                                   = 0;
//    virtual void PostWrite()                                  = 0;
//    virtual void PreRead()                                    = 0;
//    virtual void PostWrite()                                  = 0;
//    // or use:                                                
//                                                              
//    // DMA Access stubs                                       
//    virtual error_t Write(const void * buffer, size_t length) = 0;
//    virtual error_t Read(void * buffer, size_t length)        = 0;
//
//    virtual void Deallocate() = 0;
//};

LIBLINUX_SYM error_t GetLinuxMemoryInterface(const OUncontrollableRef<OLMemoryInterface> interface);