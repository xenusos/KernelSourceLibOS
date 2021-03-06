/*
    Purpose: x86_64 linux memory interfaces
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#pragma once
#include <Core/Memory/Linux/OLinuxMemory.hpp>

class IVMManager;
class OLMemoryAllocationImpl : public Memory::OLMemoryAllocation
{
public:
    OLMemoryAllocationImpl(chain_p chain, IVMManager * mngr, void * region, size_t start, size_t end, size_t size, size_t pages);
    // Important notes:
    //  The following functions are O(N) NOT O(log(n)) or better - relative to injected pages, not ::SizeInPages() 
    //  You may not insert NULL or physical addresses into the kernel; you may use the OLVirtualAddressSpace interface for phys -> kernel mapping.

    void    SetTrapHandler(Memory::OLTrapHandler_f cb, void * data)                                        override;

    bool    PageIsPresent(size_t idx)                                                                      override;
    error_t PageInsert(size_t idx, Memory::OLPageEntry page)                                               override;
    error_t PagePhysAddr(size_t idx, phys_addr_t & addr)                                                   override;
    error_t PageGetMapping(size_t idx, Memory::OLPageEntry & page)                                         override;

    size_t  SizeInPages()                                                                                  override;
    size_t  SizeInBytes()                                                                                  override;

    size_t  GetStart()                                                                                     override;
    size_t  GetEnd()                                                                                       override;

    void    ForceLinger()                                                                                  override;

    bool    IsLingering();
    IVMManager * GetMM();
protected:
    void InvalidateImp() override;

private:
    // fuck it just use DP
    IVMManager * _inject;
    bool _lingering;
    size_t _start;
    size_t _end;
    size_t _size;
    size_t _pages;
    void * _region;

    chain_p _entries;
};


extern error_t GetNewMemAllocation(bool kern, task_k task, size_t start, size_t pages, Memory::OLMemoryAllocation * & out);
