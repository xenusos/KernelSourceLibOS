/*
    Purpose: implements a common OLMemoryInterfaceImpl (depends on DP injection of OLinuxMemoryKernel.cpp or OLinuxMemoryUser.cpp interfaces)
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#include <libos.hpp>
#include "OLinuxMemory.hpp"
#include "OLinuxMemoryMM.hpp"

struct TrackedPageEntry
{
    OLPageEntry entry;
    void * handle;
    void * vm;
    OLMemoryAllocationImpl * requester;
};

OLMemoryAllocationImpl::OLMemoryAllocationImpl(chain_p chain, OLMemoryManager * mngr, void * region, size_t start, size_t end, size_t size, size_t pages)
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

void    OLMemoryAllocationImpl::SetTrapHandler(OLTrapHandler_f cb, void * data)
{
    _inject->SetCallbackHandler(_region, cb, data);
}

bool    OLMemoryAllocationImpl::PageIsPresent(size_t idx)
{
    if (ERROR(chain_get(_entries, idx, NULL, NULL)))
        return false;
    return true;
}

void CleanUpPageEntry(uint64_t hash, void * buffer)
{
    TrackedPageEntry * entry;
    OLMemoryAllocationImpl * requester;
    error_t err;
   
    entry     = (TrackedPageEntry *)buffer;
    requester = entry->requester;

    if (requester->IsLingering())
        return;

    err = requester->GetMM()->RemoveAt(entry->vm, entry->handle);
    ASSERT(NO_ERROR(err), "couldn't remove VM entry %zx", err);
}

error_t OLMemoryAllocationImpl::PageInsert(size_t idx, OLPageEntry page)
{
    error_t err;
    void * handle;
    link_p link;
    TrackedPageEntry * entry;

    // TODO: check in rnage

    err = chain_get(_entries, idx, &link, (void **) &entry);

    if ((ERROR(err)) && (err != XENUS_ERROR_LINK_NOT_FOUND))
        return err;

    if (err == XENUS_ERROR_LINK_NOT_FOUND)
    {
        err = chain_allocate_link(_entries, idx, sizeof(TrackedPageEntry), CleanUpPageEntry, &link, (void **)&entry);
    
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
    ASSERT(NO_ERROR(err), "couldn't clean up handle; fatal error: %zx", err);
    return realError;
}

error_t OLMemoryAllocationImpl::PagePhysAddr(size_t idx, phys_addr_t & addr)
{
    error_t err;
    OLPageEntry page;

    addr = (void *)0xDEADBEEFDEADBEEF;
    
    if (ERROR(err = PageGetMapping(idx, page)))
        return err;

    if (page.type == kPageEntryDummy)
    {
        addr = nullptr;
        return kStatusOkay;
    }

    if (page.type == kPageEntryByAddress)
    {
        addr = page.address;
        return kStatusOkay;
    }

    if (page.type == kPageEntryByPage)
    {
        addr = pfn_to_phys(page_to_pfn(page.page));
        return kStatusOkay;
    }

    return kErrorInternalError;
}

error_t OLMemoryAllocationImpl::PageGetMapping(size_t idx, OLPageEntry & page)
{
    error_t err;
    TrackedPageEntry * entry;

    err = chain_get(_entries, idx, NULL, (void **)&entry);

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

OLMemoryManager * OLMemoryAllocationImpl::GetMM()
{
    return _inject;
}

void OLMemoryAllocationImpl::InvalidateImp()
{
    error_t err;
    
    err = chain_destory(_entries);
    ASSERT(NO_ERROR(err), "fatal error %zx", err);

    if (_lingering)
        return;

    _inject->FreeZone(_region);
}

error_t GetNewMemAllocation(bool kern, task_k task, size_t start, size_t pages, OLMemoryAllocation * & out)
{
    error_t ret;
    OLMemoryManager * mm;
    void * priv;
    size_t trueEnd;
    size_t trueStart;
    size_t length;
    void *ree;
    chain_p chain;

    if (ERROR(ret = chain_allocate(&chain)))
        return ret;

    ree = zalloc(sizeof(OLMemoryAllocationImpl));
    if (!ree)
    {
        chain_destory(chain);
        return kErrorOutOfMemory;
    }

    if (kern)
        mm = &g_krnvm_manager;
    else
        mm = &g_usrvm_manager;

    ret = mm->AllocateZone((OLMemoryAllocationImpl *)ree, start, task, pages, &priv, trueStart, trueEnd, length);
    if (ERROR(ret))
    {
        free(ree);
        chain_destory(chain);
        return ret;
    }

    out = new(ree) OLMemoryAllocationImpl(chain, mm, priv, trueStart, trueEnd, length, pages);

    return kStatusOkay;
}
