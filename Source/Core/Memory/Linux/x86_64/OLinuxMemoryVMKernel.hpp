/*
    Purpose: x86_64 linux memory interfaces
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#pragma once

class OLMemoryManagerKernel : public OLMemoryManager
{
public:
    error_t AllocateZone(OLMemoryAllocation * space, size_t start, task_k requester, size_t pages, void ** priv, size_t & ostart, size_t & oend, size_t & length) override;
    error_t FreeZone(void * priv) override;

    void SetCallbackHandler(void * priv, OLTrapHandler_f cb, void * context) override
    {
        panic("Unsupported operation");
    }

    error_t InsertAt(void * instance, size_t index, void ** map, OLPageEntry entry) override;
    error_t RemoveAt(void * instance, void * map) override;
};


class OLKernelVirtualAddressSpaceImpl : public OLVirtualAddressSpace
{
public:

    PhysAllocationElem * AllocatePFNs(OLPageLocation location, size_t cnt, bool contig, size_t flags)       override;
    PhysAllocationElem * AllocatePages(OLPageLocation location, size_t cnt, bool contig, size_t flags)      override;
    void                 FreePages    (PhysAllocationElem * pages)                                          override;

    error_t  MapPhys      (phys_addr_t phys, size_t pages, size_t & address, void * & context)              override;
    error_t  UnmapPhys    (void * context)                                                                  override;
                                                                                                            
    error_t  MapPage      (page_k page, size_t & address, void * & context)                                 override;        
    error_t  UnmapPage    (void * context)                                                                  override;

    error_t  NewDescriptor(size_t start, size_t pages, const OOutlivableRef<OLMemoryAllocation> allocation) override;
};

extern OLMemoryManagerKernel g_krnvm_manager;

extern void InitKernVMMemory();
