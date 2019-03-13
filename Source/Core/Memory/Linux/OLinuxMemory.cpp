/*
    Purpose: x86_64 linux memory interfaces
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#define DANGEROUS_PAGE_LOGIC
#include <libos.hpp>
#include "OLinuxMemory.hpp"

static l_unsigned_long page_offset_base = 0;
static OLMemoryInterface * linux_memory = 0;

#include "../../Processes/OProcesses.hpp"
#define MEMORY_DEVICE "XenusMemoryMapper"

static void * special_map_fault;
static void * special_map_handle;
static vm_special_mapping_k special_map;

DEFINE_SYSV_FUNCTON_START(special_map_fault, size_t)
const vm_special_mapping_k sm,
vm_area_struct_k vma,
vm_fault_k vmf,
void * pad,
DEFINE_SYSV_FUNCTON_END_DEF(special_map_fault, size_t)
{
    LogPrint(LoggingLevel_e::kLogError, "something bad happened. fault at @ %p in task_struct %p", vm_fault_get_address_size_t(vmf), OSThread);
    SYSV_FUNCTON_RETURN(0)
}
DEFINE_SYSV_END



error_t GetLinuxMemoryInterface(const OUncontrollableRef<OLMemoryInterface> interface)
{
    //interface.SetObject(nullptr);
    return kStatusOkay;
}

void InitMemorySpecialMap()
{
    error_t err;

    special_map = zalloc(vm_special_mapping_size());
    ASSERT(special_map, "allocate special mapping handler");

    err = dyncb_allocate_stub(SYSV_FN(special_map_fault), 4, NULL, &special_map_fault, &special_map_handle);
    ASSERT(NO_ERROR(err), "couldn't create Xenus memory map fault handler");

    vm_special_mapping_set_fault_size_t(special_map, size_t(special_map_fault));
    vm_special_mapping_set_name_size_t(special_map,  size_t("XenusMemoryArea"));
}

void InitMemmory()
{
    InitMemorySpecialMap();

    page_offset_base = *(l_unsigned_long*)kallsyms_lookup_name("page_offset_base");
    ASSERT(special_map, "couldn't allocate special mapping struct");

    //linux_memory = new OLMemoryInterfaceImpl();
    //ASSERT(linux_memory, "couldn't allocate static memory interface");

#if defined(AMD64)
    __cachemode2pte_tbl = (uint16_t *)kallsyms_lookup_name("__cachemode2pte_tbl");
    ASSERT(__cachemode2pte_tbl, "couldn't find x86 cache lookup table");
#endif
}
