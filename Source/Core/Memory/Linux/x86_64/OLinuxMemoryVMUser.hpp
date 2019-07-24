/*
    Purpose: x86_64 linux memory interfaces
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#pragma once

struct AddressSpaceUserPrivate;

class OLMemoryManagerUser : public OLMemoryManager
{
public:
    error_t AllocateZone(OLMemoryAllocation * space, size_t start, task_k requester, size_t pages, void ** priv, size_t & ostart, size_t & oend, size_t & length) override;
    error_t FreeZoneMapping(void * priv) override;
    void FreeZoneContext(void  * priv) override;
    void SetCallbackHandler(void * priv, OLTrapHandler_f cb, void * context) override;

    error_t InsertAt(void * instance, size_t index, void ** map, OLPageEntry entry) override;
    error_t RemoveAt(void * instance, void * map) override;

    static error_t MappingAllocate(AddressSpaceUserPrivate * context);
    static void MappingFree(AddressSpaceUserPrivate * context);
    static error_t MappingTryInsert(AddressSpaceUserPrivate * context);
    static bool CheckArea(mm_struct_k mm, size_t start, size_t length, size_t & found);
    static size_t AllocateRegion(mm_struct_k mm, task_k tsk, size_t length);

    static void UnmapSpecial(AddressSpaceUserPrivate * context);
};

class OLUserVirtualAddressSpaceImpl : public OLVirtualAddressSpace
{
public:

    OLUserVirtualAddressSpaceImpl(task_k task);
    
    PhysAllocationElem * AllocatePFNs (OLPageLocation location, size_t cnt, bool contig, size_t flags)         override;
    PhysAllocationElem * AllocatePages(OLPageLocation location, size_t cnt, bool contig, size_t flags)         override;
    void                 FreePages    (PhysAllocationElem * pages)                                             override;

    error_t  MapPhys      (phys_addr_t phys, size_t pages, size_t & address, void * & context)                 override;
    error_t  UnmapPhys    (void * context)                                                                     override;
                                                                                                               
    error_t  MapPage      (page_k page, size_t & address, void * & context)                                    override;
    error_t  UnmapPage    (void * context)                                                                     override;
                                                                                                               
    error_t NewDescriptor(size_t start, size_t pages, const OOutlivableRef<OLMemoryAllocation> allocation)     override;

protected:
    void InvalidateImp() override;

private:

    task_k _task;
};

extern OLMemoryManagerUser g_usrvm_manager;


extern void InitUserVMMemory();
