/*
    Purpose: implements a common OLMemoryInterfaceImpl (depends on DP injection of OLinuxMemoryKernel.cpp or OLinuxMemoryUser.cpp interfaces)
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#include <libos.hpp>
#include "VMAllocation.hpp"           // this
#include "IVMManager.hpp"             // common interface
#include "Kernel/KernelVMManager.hpp" // interface implementations 
#include "User/UserVMManager.hpp"     // interface implementations 
#include "../OLinuxMemoryMM.hpp"      // common page io utils

struct TrackedPageEntry
{
    Memory::OLPageEntry entry;
    void * handle;
    void * vm;
    OLMemoryAllocationImpl * requester;
};

static void CleanUpPageEntry(uint64_t hash, void * buffer)
{
    TrackedPageEntry * entry = reinterpret_cast<TrackedPageEntry *>(buffer);
    OLMemoryAllocationImpl * requester;
    error_t err;

    requester = entry->requester;

    if (requester->IsLingering()) 
        return;

    err = requester->GetMM()->RemoveAt(entry->vm, entry->handle);
    ASSERT(NO_ERROR(err), "couldn't remove VM entry %zx", err);
}

OLMemoryAllocationImpl::OLMemoryAllocationImpl(chain_p chain, IVMManager * mngr, void * region, size_t start, size_t end, size_t size, size_t pages)
{
    _start     = start;
    _end       = end;
    _size      = size;
    _inject    = mngr;
    _pages     = pages;
    _region    = region;
    _entries   = chain;
    _lingering = false;
}

void OLMemoryAllocationImpl::SetTrapHandler(Memory::OLTrapHandler_f cb, void * data)
{
    _inject->SetCallbackHandler(_region, cb, data);
}

bool OLMemoryAllocationImpl::PageIsPresent(size_t idx)
{
    error_t err;

    if (idx >= _pages)
        return false;

    err = chain_get(_entries, idx, NULL, NULL);
    if (err == kErrorLinkNotFound)
        return false;

    ASSERT(NO_ERROR(err), "Couldn't check for page entry: " PRINTF_ERROR, err);
    
    return true;
}

error_t OLMemoryAllocationImpl::PageInsert(size_t idx, Memory::OLPageEntry page)
{
    error_t err;
    void * handle;
    link_p link;
    TrackedPageEntry * entry;

    if (idx >= _pages)
        return kErrorPageOutOfRange;

    err = chain_get(_entries, idx, &link, (void **) &entry);

    if ((ERROR(err)) &&
        (err != kErrorLinkNotFound))
        return err;

    if (err == kErrorLinkNotFound)
    {
        err = chain_allocate_link(_entries, idx, sizeof(TrackedPageEntry), CleanUpPageEntry, &link, reinterpret_cast<void **>(&entry));
        if (ERROR(err))
            return err;
    }
    else
    {
        err = _inject->RemoveAt(entry->vm, entry->handle);
        if (ERROR(err))
            goto fatalErrorRemoveHandle;
    }

    err = _inject->InsertAt(_region, idx, &handle, page);
    if (ERROR(err))
        goto fatalErrorRemoveHandle;

    entry->entry     = page;
    entry->vm        = _region;
    entry->handle    = handle;
    entry->requester = this;
    return kStatusOkay;

fatalErrorRemoveHandle:
    error_t realError = err;
    err = chain_deallocate_handle(link);
    ASSERT(NO_ERROR(err), "couldn't clean up handle; fatal error: " PRINTF_ERROR, err);
    return realError;
}

error_t OLMemoryAllocationImpl::PagePhysAddr(size_t idx, phys_addr_t & addr)
{
    error_t err;
    Memory::OLPageEntry page;

    addr = reinterpret_cast<void *>(0xDEADBEEFDEADBEEF);

    if (idx >= _pages)
        return kErrorPageOutOfRange;

    err = PageGetMapping(idx, page);
    if (ERROR(err))
        return err;

    if (page.type == Memory::kPageEntryDummy)
    {
        addr = nullptr;
        return kStatusOkay;
    }

    if (page.type == Memory::kPageEntryByAddress)
    {
        addr = page.address;
        return kStatusOkay;
    }

    if (page.type == Memory::kPageEntryByPage)
    {
        addr = pfn_to_phys(page_to_pfn(page.page));
        return kStatusOkay;
    }

    if (page.type == Memory::kPageEntryByPFN)
    {
        addr = pfn_to_phys(page.pfn);
        return kStatusOkay;
    }

    return kErrorInternalError;
}

error_t OLMemoryAllocationImpl::PageGetMapping(size_t idx, Memory::OLPageEntry & page)
{
    error_t err;
    TrackedPageEntry * entry;

    err = chain_get(_entries, idx, NULL, reinterpret_cast<void **>(&entry));

    if (ERROR(err))
        return err;

    page = entry->entry;
    return kStatusOkay;
}

size_t  OLMemoryAllocationImpl::SizeInPages()
{
    return _pages;
}

size_t  OLMemoryAllocationImpl::SizeInBytes()
{
    return _size;
}

size_t  OLMemoryAllocationImpl::GetStart()
{
    return _start;
}

size_t  OLMemoryAllocationImpl::GetEnd()
{
    return _end;
}

void    OLMemoryAllocationImpl::ForceLinger()
{
    _lingering = true;
}

bool   OLMemoryAllocationImpl::IsLingering()
{
    return _lingering;
}

IVMManager * OLMemoryAllocationImpl::GetMM()
{
    return _inject;
}

void OLMemoryAllocationImpl::InvalidateImp()
{
    error_t err;
    
    err = chain_destroy(_entries);
    ASSERT(NO_ERROR(err), "fatal error %zx", err);

    if (!_lingering)
        _inject->FreeZoneMapping(_region);

    _inject->FreeZoneContext(_region);
}

error_t GetNewMemAllocation(bool kern, task_k task, size_t start, size_t pages, Memory::OLMemoryAllocation * & out)
{
    void * priv;
    error_t ret;
    size_t length;
    chain_p chain;
    size_t trueEnd;
    size_t trueStart;
    IVMManager * mm;
    OLMemoryAllocationImpl *ree;

    ret = chain_allocate(&chain);
    if (ERROR(ret))
        return ret;

    ree = reinterpret_cast<OLMemoryAllocationImpl *>(zalloc(sizeof(OLMemoryAllocationImpl)));
    if (!ree)
    {
        chain_destroy(chain);
        return kErrorOutOfMemory;
    }

    if (kern)
        mm = &g_krnvm_manager;
    else
        mm = &g_usrvm_manager;

    ret = mm->AllocateZone(ree, start, task, pages, &priv, trueStart, trueEnd, length);
    if (ERROR(ret))
    {
        free(ree);
        chain_destroy(chain);
        return ret;
    }

    out = new(ree) OLMemoryAllocationImpl(chain, mm, priv, trueStart, trueEnd, length, pages);

    return kStatusOkay;
}
