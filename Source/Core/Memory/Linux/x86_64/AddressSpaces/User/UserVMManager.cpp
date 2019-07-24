/*
    Purpose: x86_64 linux memory interfaces
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#define DANGEROUS_PAGE_LOGIC
#include "Common.hpp"
#include "../IVMManager.hpp"
#include "UserVMManager.hpp"
#include "FindFreeUserVMA.hpp"
#include <Source/Core/Processes/OProcessHelpers.hpp>
#include <Core/Utilities/OThreadUtilities.hpp>
#include <Core/CPU/OThread.hpp>

using namespace Memory;

IVMManagerUser g_usrvm_manager;
static page_k  user_dummy_page;

struct AddressSpaceUserPrivate
{
    task_k task;
    size_t address;
    sysv_fptr_t special_map_fault;
    void * special_map_handle;
    vm_special_mapping_k special_map;
    size_t length;
    size_t pages;
    struct
    {
        void * context;
        OLTrapHandler_f callback;
    } fault_cb;
    OLMemoryAllocation * space;
};

DEFINE_SYSV_FUNCTON_START(special_map_fault, l_int)
    const vm_special_mapping_k sm,
    vm_area_struct_k vma,
    vm_fault_k vmf,
    void * pad,
DEFINE_SYSV_FUNCTON_END_DEF(special_map_fault, l_int)
{
    AddressSpaceUserPrivate * priv = reinterpret_cast<AddressSpaceUserPrivate *>(SYSV_GET_DATA);
    IVMFault fault(vmf);
    size_t address;

    address = vm_fault_get_address_size_t(vmf);

    if (priv->fault_cb.callback)
    {
        l_int ret;
        
        ret = priv->fault_cb.callback(priv->space, address, fault, priv->fault_cb.context);
        SYSV_FUNCTON_RETURN(ret)
    }

    LogPrint(LoggingLevel_e::kLogError, "Xenus Memory Fault at %p in task_struct %p", address, OSThread);
    LogPrint(LoggingLevel_e::kLogError, " Flags: %zx", fault.GetVarFlags().GetUInt());
    SYSV_FUNCTON_RETURN(VM_FAULT_ERROR)
}
DEFINE_SYSV_END

static l_unsigned_long GetMProtectProt(OLPageEntry entry)
{
    l_unsigned_long prot = 0;

    
    if (entry.type == kPageEntryDummy)
        return 0;

    if (entry.meta.access & OL_ACCESS_READ)
        prot |= VM_READ;

    if (entry.meta.access & OL_ACCESS_WRITE)
    {
        prot |= VM_READ;
        prot |= VM_WRITE;
    }

    if (entry.meta.access & OL_ACCESS_EXECUTE)
    {
        prot |= VM_READ;
        prot |= VM_EXEC;
    }

    return prot;
}

error_t IVMManagerUser::AllocateZone(OLMemoryAllocation * space, size_t start, task_k tsk, size_t pages, void ** priv, size_t & ostart, size_t & oend, size_t & olength)
{
    error_t err;
    vm_special_mapping_k mapping = nullptr;
    AddressSpaceUserPrivate * context = nullptr;

    context = new AddressSpaceUserPrivate();
    if (!context)
        return kErrorOutOfMemory;

    err = MappingAllocate(context);
    if (ERROR(err))
        goto error;

    context->address = start;
    context->length = pages << OS_PAGE_SHIFT;
    context->space = space;
    context->pages = pages;
    context->task = tsk;

    err = MappingTryInsert(context);

    olength = context->length;
    ostart  = context->address;
    oend    = context->address + olength;

    *priv = context;
    return kStatusOkay;

error:
    MappingFree(context);
    delete context;

    return err;
}

bool IVMManagerUser::CheckArea(mm_struct_k mm, size_t start, size_t length, size_t & found)
{
    size_t check;
    size_t end;

    if (!start)
        return false;

    end = start + length;

    for (size_t check = start; start < end; check += OS_PAGE_SIZE)
    {
        if (!find_vma(mm, check))
            continue;
     
        found = check;
        return true;
    }

    return false;
}

size_t IVMManagerUser::AllocateRegion(mm_struct_k mm, task_k tsk, size_t length)
{
    bool type = RequestMappIngType(tsk);
    bool is32 = Utilities::Tasks::IsTask32Bit(tsk);
    return RequestUnmappedArea(mm, type, NULL, length, is32);
    //return (size_t)get_unmapped_area(NULL, start, length, 0, (start ? MAP_FIXED : 0));
}

error_t IVMManagerUser::MappingAllocate(AddressSpaceUserPrivate * context)
{
    error_t err;
    void * fhandle;
    sysv_fptr_t faultcb;
    vm_special_mapping_k mapping;

    mapping = zalloc(vm_special_mapping_size());
    if (!mapping)
        return kErrorOutOfMemory;

    err = dyncb_allocate_stub(SYSV_FN(special_map_fault), 4, context, &faultcb, &fhandle);
    if (ERROR(err))
    {
        free(mapping);
        mapping = nullptr;
        return err;
    }

    vm_special_mapping_set_fault_size_t(mapping, size_t(faultcb));
    vm_special_mapping_set_name_size_t(mapping, size_t("XenusMemoryArea"));

    context->special_map_handle = fhandle;
    context->special_map_fault = faultcb;
    context->special_map = mapping;
    return kStatusOkay;
}

void IVMManagerUser::MappingFree(AddressSpaceUserPrivate * context)
{
    if (context->special_map_handle)
        dyncb_free_stub(context->special_map_handle);

    if (context->special_map)
        free(context->special_map);
}

error_t IVMManagerUser::MappingTryInsert(AddressSpaceUserPrivate * context)
{
    error_t err;
    size_t check;
    mm_struct_k mm = nullptr;
    vm_area_struct_k area = nullptr;

    mm = ProcessesAcquireMM_Write(context->task);

    if (!mm)
        goto error;

    if (CheckArea(mm, context->address, context->length, check))
    {
        LogPrint(kLogVerbose, "Couldn't allocate memory at %p -> %p, a VMA already exists at %p!", context->address, context->address + context->length, check);
        err = kErrorOutOfMemory;
        goto error;
    }

    if (!context->address)
        context->address = AllocateRegion(mm, context, context->length);

    if (LINUX_PTR_ERROR(context->address) || !context->address)
        goto error;

    //| VM_DONTEXPAND
    area = _install_special_mapping(mm, (l_unsigned_long)context->address, context->length, VM_MIXEDMAP, context->special_map);
    if (!area)
        goto error;

    ProcessesReleaseMM_Write(mm);
    return kStatusOkay;

error:
    if (mm)
        ProcessesReleaseMM_Write(mm);
    return err;
}


void IVMManagerUser::FreeZoneContext(void * priv)
{
    auto context = reinterpret_cast<AddressSpaceUserPrivate *>(priv);
    MappingFree(context);
    delete context;
}

error_t IVMManagerUser::FreeZoneMapping(void * priv)
{
    auto context = reinterpret_cast<AddressSpaceUserPrivate *>(priv);
    UnmapSpecial(context);
    return kStatusOkay;
}

void IVMManagerUser::UnmapSpecial(AddressSpaceUserPrivate * context)
{
    mm_struct_k mm;
    
    mm = ProcessesAcquireMM(context->task);
    if (!mm)
        return;

    vm_munmap_ex(mm, context->address, context->length); //vm_munmap_ex -> vm_munmap -> remove_vma_list -> remove_vma kmem_cache_free(vm_area_cachep, vma);
    ProcessesMMDecrementCounter(mm);
}

void IVMManagerUser::SetCallbackHandler(void * priv, OLTrapHandler_f cb, void * data)
{
    auto context = reinterpret_cast<AddressSpaceUserPrivate *>(priv);

    context->fault_cb.callback = cb;
    context->fault_cb.context  = data;
}

static bool InjectPage(AddressSpaceUserPrivate * context, mm_struct_k mm, vm_area_struct_k cur, size_t address, l_unsigned_long prot, OLPageEntry entry)
{
    l_unsigned_long flags;
    page_k page;
    pgprot_t protection;

    flags = vm_area_struct_get_vm_flags_size_t(cur);
    flags |= VM_IO;
    vm_area_struct_set_vm_flags_size_t(cur, flags);

    if ((flags & 7) != (prot & 7))
    {
        panicf("Ok. so... we depend on mprotect to split and merge VMAs to inject pages into userspace\n"
             "this is great because vma merging, splitting, creation, etc is a major pain in the ass\n"
             "here's the problem: we tried to inject a page of protection %i that doesn't match its vma flags of %i (prot: %i)", prot, flags, flags & 7);
    }

    if ((entry.type == kPageEntryByAddress) || (entry.type == kPageEntryByPFN))
    {
        pfn_t pfn;

        if (entry.type == kPageEntryByPFN)
            pfn = entry.pfn;
        else
            pfn = phys_to_pfn(entry.address);

        if (remap_pfn_range(cur, address, pfn.val, OS_PAGE_SIZE, entry.meta.uprot))
            return false;

        return true;
    }

    if (entry.type == kPageEntryByPage)
    {
        page       = entry.page;
        protection = entry.meta.uprot;
    }
    else if (entry.type == kPageEntryDummy)
    {
        // prevent mprotect giving userspace code access to a page that it shouldn't be allowed to read (ie: kernel module updates the address space to dummy, userspace responds with a fuck no call to mprotect again, userspace then exploits, pwn, and we die :/ )
        // we should also use pkeys!
        page       = user_dummy_page;
        protection = g_memory_interface->CreatePageEntry(0, kCacheNoCache).uprot;
    }
    else
    {
        panic("illegal page entry type");
    }
    
    vm_area_struct_set_vm_page_prot_uint64(cur, protection.pgprot_);

    if (vm_insert_page(cur, address, page))
        return false;

    return true;
}

static error_t UpdateMProtectAllowance(task_k task, size_t address, l_unsigned_long prot)
{
    vm_area_struct_k cur;
    mm_struct_k mm;
    size_t flags;

    mm = ProcessesAcquireMM_Write(task);
    if (!mm)
        return kErrorInternalError;

    cur = find_vma(mm, address);
    if (!cur)
    {
        ProcessesReleaseMM_Write(mm);
        return kErrorInternalError;
    }

    flags = vm_area_struct_get_vm_flags_size_t(cur);
    flags &= ~(VM_MAYREAD | VM_MAYEXEC | VM_MAYWRITE);

    flags |= prot & VM_WRITE ? VM_MAYWRITE : 0;
    flags |= prot & VM_READ  ? VM_MAYREAD  : 0;
    flags |= prot & VM_EXEC  ? VM_MAYEXEC  : 0;

    vm_area_struct_set_vm_flags_size_t(cur, flags);
    ProcessesReleaseMM_Write(mm);
    return kStatusOkay;
}

static error_t UpdatePageEntry(AddressSpaceUserPrivate * context, size_t address, l_unsigned_long prot, OLPageEntry entry)
{
    vm_area_struct_k cur;
    mm_struct_k mm;

    mm = ProcessesAcquireMM_Write(context->task);
    if (!mm)
        return kErrorInternalError;

    cur = find_vma(mm, address);
    if (!cur)
    {
        ProcessesReleaseMM_Write(mm);
        return kErrorInternalError;
    }

    InjectPage(context, mm, cur, address, prot, entry);
    ProcessesReleaseMM_Write(mm);
    return kStatusOkay;
}

error_t IVMManagerUser::InsertAt(void * instance, size_t index, void ** map, OLPageEntry entry)
{
    auto context = reinterpret_cast<AddressSpaceUserPrivate *>(instance);
    size_t offset;
    size_t address;
    l_unsigned_long prot;
    l_int ret;
    error_t err;

    prot    = GetMProtectProt(entry);

    offset  = index << OS_PAGE_SHIFT;
    address = offset + context->address;

    // this is kinda dangerous but this hack will do for now
    // also: we release the MM. this isn't atomically safe, but it's safe enough as we can't really be exploited here
    err = UpdateMProtectAllowance(context->task, address, prot);
    if (ERROR(err))
        return err;

    // split/merge/update prot
    ret = do_mprotect_pkey(context->task, address, OS_PAGE_SIZE, prot, -1);
    if (LINUX_INT_ERROR(ret))
    {
        LogPrint(kLogError, "mprotect failed! %x", ret);
        return kErrorInternalError;
    }

    err = UpdatePageEntry(context, address, prot, entry);
    if (ERROR(err))
        return err;

    *map = (void *)address;
    return kStatusOkay;
}

error_t IVMManagerUser::RemoveAt(void * instance, void * map)
{
    void * idc;
    size_t address;
    OLPageEntry entry;
    AddressSpaceUserPrivate * context;

    address = (size_t)map;
    context = (AddressSpaceUserPrivate *)instance;
    
    entry.type = kPageEntryDummy;

    return this->InsertAt(instance, (address - context->address) >> OS_PAGE_SHIFT, &idc, entry);
}

void InitUserVMMemory()
{
    user_dummy_page  = alloc_pages_current(GFP_USER, 0);
}
