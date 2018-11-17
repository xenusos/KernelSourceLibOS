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
    virtual size_t GetVAStart()           = 0;
    virtual size_t GetVAEnd()             = 0;
    virtual size_t GetLength()            = 0;

    virtual error_t Unmap()               = 0;
};

// Describes a virtual buffer pre-page allocation
class OLBufferDescription : public OObject
{
public:
    virtual bool    PageIsPresent(int idx)                                            = 0;
    virtual error_t PageInsert(int idx, page_k page)                                  = 0;
    virtual error_t PagePhysAddr(int idx, phys_addr_t & addr)                         = 0;
    virtual error_t PageMap(int idx, void * & addr)                                   = 0;
    virtual void    PageUnmap(void * addr)                                            = 0;

    virtual bool    IsVoid()                                                          = 0;
    virtual error_t HasError()                                                        = 0;
    virtual bool    IsHandled()                                                       = 0;

    virtual error_t MapKernel(const OUncontrollableRef<OLGenericMappedBuffer> kernel, pgprot_t prot)              = 0;
    virtual error_t MapUser  (const OUncontrollableRef<OLGenericMappedBuffer> kernel, task_k task, pgprot_t prot) = 0;
};

enum OLPageLocation
{
    kPageInvalid = -1,
    kPageDMAVeryLow, // ZONE_DMA     < 16M
    kPageDMA4GB,     // ZONE_DMA32   < 2 ^ 32 -1
    kPageNormal      // ZONE_NORMAL  
};


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
// on x86, all we need to do is flush the processors cache to ensure we're synced unlike other architectures.
// dma_ops on linux builds targeting x86_64 just points to nommu_dma_ops [assuming gart hackery isn't used]
// nommu_dma_ops.sync_??_for_device = flush_write_buffers();
// 
// MSDN docs just state "idk man- PAGED BUFFERS" without actually defining what makes dma buffers from a dma device special
// the dxgk subsystem just allocates generic no-cache PTEs for an MDL 
//
// we could implement an OLDmaBuffer to be safe, but since 90% of Xenus is x86_64 only, there is no point

LIBLINUX_SYM error_t GetLinuxMemoryInterface(const OUncontrollableRef<OLMemoryInterface> interface);