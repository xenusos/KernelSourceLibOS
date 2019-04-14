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
    // xenus(5, rax) as linux sysv x86_64:
    const uint8_t x86_64[] = { 0x48, 0xC7, 0xC7, 0x05, 0x00, 0x00, 0x00, // MOV RDI, 0x5
                               0x48, 0x89, 0xC6,                         // MOV RSI, RAX
                               0x48, 0xC7, 0xC0, 0x90, 0x01, 0x00, 0x00, // MOV RAX, 0x190 (400 a/k/a XENUSES SYSCALL)
                               0x0F, 0x05 };                             // SYSCALL

    size_t addr;
    error_t err;
    OLVirtualAddressSpace * vas;
    ODumbPointer<OLMemoryAllocation> alloc;
    OLPageEntry entry;
    page_k * pages;

    err = g_memory_interface->GetKernelAddressSpace(OUncontrollableRef<OLVirtualAddressSpace>(vas));
    ASSERT(NO_ERROR(err), "fatal error: couldn't get kernel address space interface: %zx", err);

    err = vas->NewDescriptor(0, 1, OOutlivableRef<OLMemoryAllocation>(alloc));
    ASSERT(NO_ERROR(err), "fatal error: couldn't allocate kernel address VM area: %zx", err);
    
    pages = vas->AllocatePages(OLPageLocation::kPageNormal, 1, true, OL_PAGE_ZERO);
    ASSERT(pages, "fatal error: couldn't allocate return stub");

    g_return_stub_x64 = pages[0];

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
