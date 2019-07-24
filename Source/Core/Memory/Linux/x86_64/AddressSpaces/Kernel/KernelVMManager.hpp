/*
    Purpose: x86_64 linux memory interfaces
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#pragma once

class IVMManagerKernel : public IVMManager
{
public:
    error_t AllocateZone(Memory::OLMemoryAllocation * space, size_t start, task_k requester, size_t pages, void ** priv, size_t & ostart, size_t & oend, size_t & length) override;
   
    error_t FreeZoneMapping(void * priv) override;
    void    FreeZoneContext(void  * priv) override;

    void SetCallbackHandler(void * priv, Memory::OLTrapHandler_f cb, void * context) override
    {
        panic("Unsupported operation");
    }

    error_t InsertAt(void * instance, size_t index, void ** map, Memory::OLPageEntry entry) override;
    error_t RemoveAt(void * instance, void * map) override;
};


extern IVMManagerKernel g_krnvm_manager;
extern void InitKernVMMemory();
