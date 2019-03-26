/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#include <libos.hpp>
#include "ODECriticalSection.hpp"
#include "../../Memory/Linux/OLinuxMemory.hpp"

page_k g_return_stub_x64;

static void InitDE64()
{
    const uint8_t x86_64[] = { 0x48, 0xC7, 0xC7, 0x05, 0x00, 0x00, 0x00, 0x48, 0x89, 0xC6, 0x48, 0xC7, 0xC0, 0x90, 0x01, 0x00, 0x00, 0x0F, 0x05 };

    size_t addr;
    error_t err;
    OLVirtualAddressSpace * vas;
    OLMemoryAllocation * alloc;
    OLPageEntry entry;

    err = g_memory_interface->GetKernelAddressSpace(OUncontrollableRef<OLVirtualAddressSpace>(vas));
    ASSERT(NO_ERROR(err), "fatal error: couldn't get kernel address space interface: %zx", err);

    err = vas->NewDescriptor(0, 1, OOutlivableRef<OLMemoryAllocation>(alloc));
    ASSERT(NO_ERROR(err), "fatal error: couldn't allocate kernel address VM area: %zx", err);

    g_return_stub_x64 = alloc_pages_current(GFP_KERNEL, 0);
    ASSERT(g_return_stub_x64, "ODE: InitReturnStub_64, couldn't allocate return stub");

    entry.meta = g_memory_interface->CreatePageEntry(OL_ACCESS_READ | OL_ACCESS_WRITE, kCacheNoCache);
    entry.type = kPageEntryByPage;
    entry.page = g_return_stub_x64;

    err = alloc->PageInsert(0, entry);
    ASSERT(NO_ERROR(err), "fatal error: couldn't insert page into kernel: %zx", err);

    memcpy((void *)alloc->GetStart(), x86_64, sizeof(x86_64));
}

extern void InitDEReturn()
{
    InitDE64();
}
