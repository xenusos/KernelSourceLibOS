/*
    Purpose: x86_64 linux memory interfaces
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#define DANGEROUS_PAGE_LOGIC
#include <libos.hpp>
#include "OLinuxMemory.hpp"
#include "OLinuxMemoryMM.hpp"
#include "../../Processes/OProcesses.hpp"

OLMemoryManagerUser g_usrvm_manager;
static page_k       user_dummy_page;
static sysv_fptr_t  do_mprotect_pkey;

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
    
    priv = (AddressSpaceUserPrivate *)SYSV_GET_DATA;

    if (priv->fault_cb.callback)
    {
        l_int ret;
        IVMFault fault(vmf);
        
        ret = priv->fault_cb.callback(priv->space, fault.GetVarCowPage().GetUInt(), fault, priv->fault_cb.context);
        SYSV_FUNCTON_RETURN(ret)
    }

    LogPrint(LoggingLevel_e::kLogError, "something bad happened. fault at @ %p in task_struct %p", vm_fault_get_address_size_t(vmf), OSThread);
    SYSV_FUNCTON_RETURN(0)
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

        // https://elixir.bootlin.com/linux/v4.14.106/source/mm/mprotect.c#L514
        // this could have been a pretty big YIKES!
        // note: on error, no free up operation is needed. callee owns mm & get_unmapped_area doesn't actually take owership of the area until we install the map/create a vma
        area = _install_special_mapping(mm, (l_unsigned_long)address, length, VM_GROWSUP /* | VM_MAYWRITE | VM_MAYREAD | VM_MAYEXEC | */ | VM_SHARED, mapping);
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

    mm = ProcessesAcquireMM(context->task);

    if (!mm)
        return kErrorInternalError;

    //vm_munmap_ex -> vm_munmap -> remove_vma_list -> remove_vma kmem_cache_free(vm_area_cachep, vma);
    vm_munmap_ex(mm, context->address, context->length);

    ProcessesMMDecrementCounter(mm);

    dyncb_free_stub(context->special_map_handle);
    free(context->special_map);
    delete context;
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

    flags = mm_struct_get_flags_size_t(mm);

    if (flags & 7 != prot & 7)
    {

        panicf("Ok. so... we depend on mprotect to split and merge VMAs to inject pages into userspace\n"
               "this is great because vma merging, splitting, creation, etc is a major pain in the ass \n"
               "here's the problem: we tried to inject a page of protection %i that doesn't match its vma flags of %i (prot: %i)", prot, flags, flags & 7);
    }

    flags = mm_struct_get_def_flags_size_t(mm);
    flags |= VM_DONTEXPAND | VM_SOFTDIRTY;
    flags |= VM_MAYWRITE | VM_MAYREAD | VM_MAYEXEC | VM_SHARED;
    flags |= VM_GROWSUP;
    flags |= prot;
    vm_area_struct_set_vm_flags_size_t(cur, flags);

    vm_area_struct_set_vm_page_prot_uint64(cur, entry.prot.pgprot_);

    if (entry.type == kPageEntryByAddress)
    {
        if (remap_pfn_range(cur, address, phys_to_pfn(entry.address).val, OS_PAGE_SIZE, entry.prot))
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

    if (vm_insert_page(cur, address, page))
        return false;

    return true;
}

static l_unsigned_long GetMProtectProt(OLPageEntry entry)
{
    l_unsigned_long prot = 0;

    if (entry.type == kPageEntryDummy)
        return 0;

    if (entry.access & OL_ACCESS_READ)
        prot |= VM_READ;

    if (entry.access & OL_ACCESS_WRITE)
    {
        prot |= VM_READ;
        prot |= VM_WRITE;
    }

    if (entry.access & OL_ACCESS_EXECUTE)
    {
        prot |= VM_READ;
        prot |= VM_EXEC;
    }

    return prot;
}

error_t OLMemoryManagerUser::InsertAt(void * instance, size_t index, void ** map, OLPageEntry entry)
{
    mm_struct_k mm;
    size_t offset;
    size_t address;
    l_unsigned_long prot;
    l_int ret;
    vm_area_struct_k cur;
    AddressSpaceUserPrivate * context;

    context = (AddressSpaceUserPrivate *)instance;

    mm = ProcessesAcquireMM_Write(context->task);
    if (!mm)
        return kErrorInternalError;

    prot    = GetMProtectProt(entry);

    offset  = index << OS_PAGE_SHIFT;
    address = offset + context->address;

    // mprotect will merge and split the vmas by protection for us :DDD
    ret = ez_linux_caller(do_mprotect_pkey, address, OS_PAGE_SIZE, prot, 0xffffffffffffffff, 0, 0, 0, 0, 0, 0, 0, 0);
    if (!ret)
    {
        LogPrint(kLogError, "mprotect failed!");
        goto error;
    }

    cur = find_vma(mm, address);
    if (!cur)
    {
        LogPrint(kLogError, "VMA not found!");
        goto error;
    }

    InjectPage(context, mm, cur, address, prot, entry);

okay:
    *map = (void *)address;

    ProcessesReleaseMM_Write(mm);
    return kStatusOkay;

error:
    ProcessesReleaseMM_Write(mm);
    return kErrorInternalError;
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

    return this->InsertAt(instance, (context->address - address) >> OS_PAGE_SHIFT, &idc, entry);
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

error_t  OLUserVirtualAddressSpaceImpl::MapPage(page_k page, size_t pages, size_t & address, void * & context)
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
    user_dummy_page  = alloc_pages_current(GFP_KERNEL, 0);
    do_mprotect_pkey = (sysv_fptr_t) kallsyms_lookup_name("do_mprotect_pkey");
}
