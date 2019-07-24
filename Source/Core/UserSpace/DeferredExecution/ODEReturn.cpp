/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#include <libos.hpp>
#include "../../Memory/Linux/OLinuxMemory.hpp"
#include "../DelegatedCalls/ODelegtedCalls.hpp"

static page_k return_stub_x86_64;
static page_k return_stub_x86_32;

static page_k DEAllocatePage(const void * buffer, size_t length)
{
    size_t addr;
    error_t err;
    Memory::OLVirtualAddressSpace * vas;
    ODumbPointer<Memory::OLMemoryAllocation> alloc;
    Memory::OLPageEntry entry;
    Memory::PhysAllocationElem * allocation;
    page_k page;

    err = g_memory_interface->GetKernelAddressSpace(OUncontrollableRef<Memory::OLVirtualAddressSpace>(vas));
    ASSERT(NO_ERROR(err), "fatal error: couldn't get kernel address space interface: %zx", err);

    err = vas->NewDescriptor(0, 1, OOutlivableRef<Memory::OLMemoryAllocation>(alloc));
    ASSERT(NO_ERROR(err), "fatal error: couldn't allocate kernel address VM area: %zx", err);

    allocation = vas->AllocatePages(Memory::OLPageLocation::kPageNormal, 1, true, Memory::OL_PAGE_ZERO);
    ASSERT(allocation, "fatal error: couldn't allocate return stub");

    page = allocation[0].page;

    entry.meta = g_memory_interface->CreatePageEntry(Memory::OL_ACCESS_READ | Memory::OL_ACCESS_WRITE, Memory::kCacheNoCache);
    entry.type = Memory::kPageEntryByPage;
    entry.page = page;

    err = alloc->PageInsert(0, entry);
    ASSERT(NO_ERROR(err), "fatal error: couldn't insert page into kernel: %zx", err);

    memcpy((void *)alloc->GetStart(), buffer, length);

    return page;
}

static void InitDE64()
{
    // xenus(5, rax) as linux sysv x86_64:
    const uint8_t x86_64[] = { 0x48, 0xC7, 0xC7, BUTLTIN_CALL_NTFY_COMPLETE, 0x00, 0x00, 0x00, // MOV RDI, 0x5
                               0x48, 0x89, 0xC6,                                               // MOV RSI, RAX
                               0x48, 0xC7, 0xC0, 0x90, 0x01, 0x00, 0x00,                       // MOV RAX, 0x190 (400 a/k/a XENUSES SYSCALL)
                               0x0F, 0x05 };                                                   // SYSCALL

    return_stub_x86_64 = DEAllocatePage(x86_64, sizeof(x86_64));
}


static void InitDE32()
{
    // xenus(5, eax) as linux sysv x86_32:
    const uint8_t x86_32[] = { 0xBB, BUTLTIN_CALL_NTFY_COMPLETE, 0x00, 0x00, 0x00,             // MOV EBX, 0x5
                               0x89, 0xC1,                                                     // MOV ECX, EAX
                               0xB8, 0x90, 0x01, 0x00, 0x00,                                   // MOV EAX, 0x190 (400 a/k/a XENUSES SYSCALL)
                               0xCD, 0x80 };                                                   // INT 0x80

    return_stub_x86_32 = DEAllocatePage(x86_32, sizeof(x86_32));
}

page_k DEGetReturnStub(bool bits64)
{
    return bits64 ? return_stub_x86_64 : return_stub_x86_32;
}

void InitDEReturn()
{
    InitDE64();
    InitDE32();
}
