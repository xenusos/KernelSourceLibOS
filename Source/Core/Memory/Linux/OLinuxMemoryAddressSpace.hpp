/*
    Purpose: x86_64 linux memory interfaces
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#pragma once

class OLMemoryAllocationImpl : public OLMemoryAllocation
{
public:
    OLMemoryAllocationImpl(OLMemoryManager * mngr, void * region, size_t start, size_t end, size_t size, size_t pages);
    // Important notes:
    //  The following functions are O(N) NOT O(log(n)) or better - relative to injected pages, not ::SizeInPages() 
    //  You may not insert NULL or physical addresses into the kernel; you may use the OLVirtualAddressSpace interface for phys -> kernel mapping.

    void    SetTrapHandler(OLTrapHandler_f cb, void * data)                                                override;

    bool    PageIsPresent(size_t idx)                                                                      override;
    error_t PageInsert(size_t idx, OLPageEntry page)                                                       override;
    error_t PagePhysAddr(size_t idx, phys_addr_t & addr)                                                   override;
    error_t PageGetMapping(size_t idx, OLPageEntry & page)                                                 override;

    size_t  SizeInPages()                                                                                  override;
    size_t  SizeInBytes()                                                                                  override;

    size_t  GetStart()                                                                                     override;
    size_t  GetEnd()                                                                                       override;

    void    ForceLinger()                                                                                  override;

protected:
    void InvalidateImp() override;

private:
    // fuck it just use DP
    OLMemoryManager * _inject;
    bool _lingering = false;
    size_t _start;
    size_t _end;
    size_t _size;
    size_t _pages;
    void * _region;
};


extern error_t GetNewMemAllocation(bool kern, task_k task, size_t start, size_t pages, OLMemoryAllocation * & out);
