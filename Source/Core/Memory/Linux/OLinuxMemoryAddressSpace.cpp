/*
    Purpose: implements a common OLMemoryInterfaceImpl (depends on DP injection of OLinuxMemoryKernel.cpp or OLinuxMemoryUser.cpp interfaces)
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#include <libos.hpp>
#include "OLinuxMemory.hpp"


OLMemoryAllocationImpl::OLMemoryAllocationImpl(OLMemoryManager * mngr, void * region, size_t start, size_t end, size_t size, size_t pages)
{
    _start  = start;
    _end    = end;
    _size   = size;
    _inject = mngr;
    _pages  = pages;
    _region = region;
}


void    OLMemoryAllocationImpl::SetTrapHandler(OLTrapHandler_f cb, void * data)
{
    _inject->SetCallbackHandler(_region, cb, data);
}

bool    OLMemoryAllocationImpl::PageIsPresent(size_t idx)
{

}

error_t OLMemoryAllocationImpl::PageInsert(size_t idx, OLPageEntry page)
{

}

error_t OLMemoryAllocationImpl::PagePhysAddr(size_t idx, phys_addr_t & addr)
{

}

error_t OLMemoryAllocationImpl::PageGetMapping(size_t idx, OLPageEntry & page)
{

}

size_t  OLMemoryAllocationImpl::SizeInPages()
{

}

size_t  OLMemoryAllocationImpl::SizeInBytes()
{

}

size_t  OLMemoryAllocationImpl::GetStart()
{

}

size_t  OLMemoryAllocationImpl::GetEnd()
{

}

void    OLMemoryAllocationImpl::ForceLinger()
{
    _lingering = true;
}

void OLMemoryAllocationImpl::InvalidateImp()
{

    if (_lingering)
        return;

    _inject->FreeZone(_region);
}

void* __cdecl operator new(size_t idc, void * placement)
{
    return placement;
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

    ree = zalloc(sizeof(OLMemoryAllocationImpl));
    if (!ree)
        return kErrorOutOfMemory;

    if (kern)
        mm = &g_krnvm_manager;
    else
        mm = &g_usrvm_manager;

    ret = mm->AllocateZone((OLMemoryAllocationImpl *)ree, start, task, pages, &priv, trueStart, trueEnd, length);
    
    if (ERROR(ret))
    {
        free(ree);
        return ret;
    }

    out = new(ree) OLMemoryAllocationImpl(mm, priv, trueStart, trueEnd, length, pages);

    return kStatusOkay;
}
