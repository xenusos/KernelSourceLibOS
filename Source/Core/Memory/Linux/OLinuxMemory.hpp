/*
    Purpose: x86_64 linux memory interfaces
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#pragma once

#include <Core/Memory/Linux/OLinuxMemory.hpp>

class OLMemoryManager
{
public:
    virtual error_t InsertAt(size_t address, void ** priv, OLPageEntry entry) = 0;
    virtual error_t FreeVMAs(void * priv)                                     = 0;
};

class OLMemoryAllocationImpl : public OLMemoryAllocation
{
public:
    OLMemoryAllocationImpl(OLMemoryManager * mngr)
    {
        _inject = mngr;
    }

    // Important notes:
    //  The following functions are O(N) NOT O(log(n)) or better - relative to injected pages, not ::SizeInPages() 
    //  You may not insert NULL or physical addresses into the kernel; you may use the OLVirtualAddressSpace interface for phys -> kernel mapping.

    bool    PageIsPresent (size_t idx)                                                                     override;
    error_t PageInsert    (size_t idx, OLPageEntry page)                                                   override;
    error_t PagePhysAddr  (size_t idx, phys_addr_t & addr)                                                 override;
    error_t PageGetMapping(size_t idx, OLPageEntry & page)                                                 override;
                                                                                                           
    size_t  SizeInPages()                                                                                  override;
    size_t  SizeInBytes()                                                                                  override;
                                                                                                           
    size_t  GetStart()                                                                                     override;
    size_t  GetEnd()                                                                                       override;
                                                                                                           
    void    ForceLinger()                                                                                  override;

private:
    // fuck it just use DP
    OLMemoryManager * _inject;
};

class OLUserVirtualAddressSpaceImpl : public OLVirtualAddressSpace
{
public:

    page_k * AllocatePages(OLPageLocation location, size_t cnt, size_t flags = 0)                          override;
    void         FreePages(page_k * pages)                                                                 override;

    error_t        MapPhys(phys_addr_t phys, size_t pages, size_t & address, void * & context)             override;
    error_t      UnmapPhys(void * context)                                                                 override;

    error_t        MapPage(page_k page, size_t pages, size_t & address, void * & context)                  override;
    error_t      UnmapPage(void * context)                                                                 override;

    error_t NewDescriptor(size_t start, size_t pages, const OOutlivableRef<OLMemoryAllocation> allocation) override;
};

class OLKernelVirtualAddressSpaceImpl : public OLVirtualAddressSpace
{
public:

    page_k * AllocatePages(OLPageLocation location, size_t cnt, size_t flags = 0)                          override;
    void         FreePages(page_k * pages)                                                                 override;

    error_t        MapPhys(phys_addr_t phys, size_t pages, size_t & address, void * & context)             override;
    error_t      UnmapPhys(void * context)                                                                 override;

    error_t        MapPage(page_k page, size_t pages, size_t & address, void * & context)                  override;
    error_t      UnmapPage(void * context)                                                                 override;

    error_t NewDescriptor(size_t start, size_t pages, const OOutlivableRef<OLMemoryAllocation> allocation) override;
};

class OLMemoryInterfaceImpl : public OLMemoryInterface
{
public:
    OLPageLocation  GetPageLocation   (size_t max)                                                         override;
    size_t          GetPageRegionStart(OLPageLocation location)                                            override; 
    size_t          GetPageRegionEnd  (OLPageLocation location)                                            override;


    phys_addr_t     PhysPage(page_k page)                                                                  override;

    void            UpdatePageEntryCache (OLPageEntryMeta &, OLCacheType cache)                            override;
    void            UpdatePageEntryAccess(OLPageEntryMeta &, size_t access)                                override;
    OLPageEntryMeta CreatePageEntry      (size_t access, OLCacheType cache)                                override;

    error_t GetKernelAddressSpace(const OUncontrollableRef<OLVirtualAddressSpace> builder)                 override;
    error_t GetUserAddressSpace(const OUncontrollableRef<OLVirtualAddressSpace> builder)                   override;
};

extern void InitMemmory();
