/*
    Purpose: x86_64 linux memory interfaces
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#define DANGEROUS_PAGE_LOGIC
#include <libos.hpp>
#include "OLinuxMemoryVM.hpp"

#include "OLinuxMemoryMM.hpp"
#include "../OLinuxMemory.hpp"
#include "../../../Processes/OProcesses.hpp"

OLMemoryManagerUser g_usrvm_manager;
static page_k       user_dummy_page;

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
    AddressSpaceUserPrivate * priv;
    IVMFault fault(vmf);
    size_t address;
    
    priv = (AddressSpaceUserPrivate *)SYSV_GET_DATA;

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

error_t OLMemoryManagerUser::AllocateZone(OLMemoryAllocation * space, size_t start, task_k tsk, size_t pages, void ** priv, size_t & ostart, size_t & oend, size_t & olength)
{
    error_t err;
    void * fhandle;
    sysv_fptr_t faultcb;
    vm_special_mapping_k mapping;
    AddressSpaceUserPrivate * context;
    mm_struct_k mm;
    size_t address;
    vm_area_struct_k area;
    size_t length;

    context = new AddressSpaceUserPrivate();
    if (!context)
        return kErrorOutOfMemory;

    mapping = zalloc(vm_special_mapping_size());
    if (!mapping)
    {
        delete context;
        return kErrorOutOfMemory;
    }

    err = dyncb_allocate_stub(SYSV_FN(special_map_fault), 4, context, &faultcb, &fhandle);
    if (ERROR(err))
    {
        free(mapping);
        delete context;
        return err;
    }

    vm_special_mapping_set_fault_size_t(mapping, size_t(faultcb));
    vm_special_mapping_set_name_size_t(mapping,  size_t("XenusMemoryArea"));

    length = pages << OS_PAGE_SHIFT;

    {
        mm = ProcessesAcquireMM_Write(tsk);
        
        if (!mm)
            goto errorNoClean;
        
        address = (size_t)get_unmapped_area(NULL, start, length, 0, start ? MAP_FIXED : 0);
        if (!address)
            goto error;

        area = _install_special_mapping(mm, (l_unsigned_long)address, length, 0, mapping);
        if (!area)
            goto error; 

        ProcessesReleaseMM_Write(mm);
    }

    context->special_map_handle = fhandle;
    context->special_map_fault = faultcb;
    context->special_map = mapping;
    context->address = address;
    context->length = length;
    context->space = space;
    context->pages = pages;
    context->task = tsk;

    olength = length;
    ostart  = address;
    oend    = address + olength;

    *priv = context;
    return kStatusOkay;

error:
    ProcessesReleaseMM_Write(mm);

errorNoClean:
    dyncb_free_stub(fhandle);
    free(mapping);
    delete context;

    return err;
}

error_t OLMemoryManagerUser::FreeZone(void * priv)
{
    mm_struct_k mm;
    AddressSpaceUserPrivate * context;

    context = (AddressSpaceUserPrivate *)priv;
    
    {
        mm = ProcessesAcquireMM(context->task);
        if (!mm)
            return kErrorInternalError;

        vm_munmap_ex(mm, context->address, context->length); //vm_munmap_ex -> vm_munmap -> remove_vma_list -> remove_vma kmem_cache_free(vm_area_cachep, vma);

        ProcessesMMDecrementCounter(mm);
    }
    
    dyncb_free_stub(context->special_map_handle);
    free(context->special_map);
    delete context;
    
    return kStatusOkay;
}

void OLMemoryManagerUser::SetCallbackHandler(void * priv, OLTrapHandler_f cb, void * data)
{
    AddressSpaceUserPrivate * context;

    context = (AddressSpaceUserPrivate *)priv;

    context->fault_cb.callback = cb;
    context->fault_cb.context  = data;
}

static bool InjectPage(AddressSpaceUserPrivate * context, mm_struct_k mm, vm_area_struct_k cur, size_t address, l_unsigned_long prot, OLPageEntry entry)
{
    l_unsigned_long flags;
    page_k page;

    flags = vm_area_struct_get_vm_flags_size_t(cur);

    if ((flags & 7) != (prot & 7))
    {
        panicf("Ok. so... we depend on mprotect to split and merge VMAs to inject pages into userspace\n"
              "this is great because vma merging, splitting, creation, etc is a major pain in the ass\n"
              "here's the problem: we tried to inject a page of protection %i that doesn't match its vma flags of %i (prot: %i)", prot, flags, flags & 7);
    }

    if (entry.type == kPageEntryByAddress)
    {
        if (remap_pfn_range(cur, address, phys_to_pfn(entry.address).val, OS_PAGE_SIZE, entry.meta.prot))
            return false;

        return true;
    }

    if (entry.type == kPageEntryByPage)
    {
        page = entry.page;
    }
    else if (entry.type == kPageEntryDummy)
    {
        // prevent mprotect giving userspace code access to a page that it shouldn't be allowed to read (ie: kernel module updates the address space to dummy, userspace responds with a fuck no call to mprotect again, userspace then exploits, pwn, and we die :/ )
        // we should also use pkeys!
        page = user_dummy_page;
    }
    else
    {
        panic("illegal page entry type");
    }

    if (vm_insert_page(cur, address, page))
        return false;

    return true;
}

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
    flags &= VM_MAYREAD;
    flags &= VM_MAYWRITE;
    flags &= VM_MAYEXEC;

    flags |= VM_GROWSUP;

    flags |= prot & VM_WRITE ? VM_MAYWRITE : 0;
    flags |= prot & VM_READ ? VM_MAYREAD : 0;
    flags |= prot & VM_EXEC ? VM_MAYEXEC : 0;

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

error_t OLMemoryManagerUser::InsertAt(void * instance, size_t index, void ** map, OLPageEntry entry)
{
    size_t offset;
    size_t address;
    l_unsigned_long prot;
    l_int ret;
    error_t err;
    AddressSpaceUserPrivate * context;

    context = (AddressSpaceUserPrivate *)instance;

    prot    = GetMProtectProt(entry);

    offset  = index << OS_PAGE_SHIFT;
    address = offset + context->address;

    // this is kinda dangerous but this hack will do for now
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

error_t OLMemoryManagerUser::RemoveAt(void * instance, void * map)
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

OLUserVirtualAddressSpaceImpl::OLUserVirtualAddressSpaceImpl(task_k task)
{
    if (!task)
    {
        LogPrint(kLogWarning, "OLUserspaceAddressSpace constructor called with null process instance... using OSThread (a/k/a current)");
        task = OSThread;
    }

    _task = task;
    
    ProcessesTaskIncrementCounter(_task);
}

void OLUserVirtualAddressSpaceImpl::InvalidateImp()
{
    ProcessesTaskDecrementCounter(_task);
}

page_k * OLUserVirtualAddressSpaceImpl::AllocatePages(OLPageLocation location, size_t cnt, bool contig, size_t flags)
{
    return AllocateLinuxPages(location, cnt, true, contig, flags);
}

void OLUserVirtualAddressSpaceImpl::FreePages(page_k * pages)
{
    FreeLinuxPages(pages);
}

error_t  OLUserVirtualAddressSpaceImpl::MapPhys(phys_addr_t phys, size_t pages, size_t & address, void * & context)
{
    // TODO:
    return kErrorNotImplemented;
}

error_t  OLUserVirtualAddressSpaceImpl::UnmapPhys(void * context)
{
    // TODO:
    return kErrorNotImplemented;
}

error_t  OLUserVirtualAddressSpaceImpl::MapPage(page_k page, size_t & address, void * & context)
{
    // TODO:
    return kErrorNotImplemented;
}

error_t  OLUserVirtualAddressSpaceImpl::UnmapPage(void * context)
{
    // TODO:
    return kErrorNotImplemented;
}

error_t OLUserVirtualAddressSpaceImpl::NewDescriptor(size_t start, size_t pages, const OOutlivableRef<OLMemoryAllocation> allocation)
{
    error_t ret;
    OLMemoryAllocation * instance;
    
    ret = GetNewMemAllocation(false, _task, start, pages, instance);
    if (ERROR(ret))
        return ret;

    allocation.PassOwnership(instance);
    return kStatusOkay;
}

void InitUserVMMemory()
{
    user_dummy_page  = alloc_pages_current(GFP_USER, 0);
}
