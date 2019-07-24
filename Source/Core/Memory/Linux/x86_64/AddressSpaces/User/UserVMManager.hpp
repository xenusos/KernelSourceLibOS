/*
    Purpose: x86_64 linux memory interfaces
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#pragma once

struct AddressSpaceUserPrivate;

class IVMManagerUser : public IVMManager
{
public:
    error_t AllocateZone(Memory::OLMemoryAllocation * space, size_t start, task_k requester, size_t pages, void ** priv, size_t & ostart, size_t & oend, size_t & length) override;
    error_t FreeZoneMapping(void * priv) override;
    void FreeZoneContext(void  * priv) override;
    void SetCallbackHandler(void * priv, Memory::OLTrapHandler_f cb, void * context) override;

    error_t InsertAt(void * instance, size_t index, void ** map, Memory::OLPageEntry entry) override;
    error_t RemoveAt(void * instance, void * map) override;

    static error_t MappingAllocate(AddressSpaceUserPrivate * context);
    static void MappingFree(AddressSpaceUserPrivate * context);
    static error_t MappingTryInsert(AddressSpaceUserPrivate * context);
    static bool CheckArea(mm_struct_k mm, size_t start, size_t length, size_t & found);
    static size_t AllocateRegion(mm_struct_k mm, task_k tsk, size_t length);

    static void UnmapSpecial(AddressSpaceUserPrivate * context);
};

extern IVMManagerUser g_usrvm_manager;

extern void InitUserVMMemory();
