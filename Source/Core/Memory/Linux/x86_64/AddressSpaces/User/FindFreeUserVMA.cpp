/*
    Purpose: 
    Author:
    License: 
*/
#define DANGEROUS_PAGE_LOGIC
#include <libos.hpp>
#include "FindFreeUserVMA.hpp"

static bool deprecated_mpx_check(va_kernel_pointer_t & addr, size_t length, size_t flags) //  mpx_unmapped_area_check
{
    return true;
}

struct vm_unmapped_area_info {
#define VM_UNMAPPED_AREA_TOPDOWN 1
#define VM_UNMAPPED_AREA_PROVIDED_MM 2
    mm_struct_k mm;
    l_unsigned_long flags;
    l_unsigned_long length;
    l_unsigned_long low_limit;
    l_unsigned_long high_limit;
    l_unsigned_long align_mask;
    l_unsigned_long align_offset;
};

enum {
    UNAME26 = 0x0020000,
    ADDR_NO_RANDOMIZE = 0x0040000,	/* disable randomization of VA space */
    FDPIC_FUNCPTRS = 0x0080000,	/* userspace function ptrs point to descriptors
                         * (signal handling)
                         */
    MMAP_PAGE_ZERO = 0x0100000,
    ADDR_COMPAT_LAYOUT = 0x0200000,
    READ_IMPLIES_EXEC = 0x0400000,
    ADDR_LIMIT_32BIT = 0x0800000,
    SHORT_INODE = 0x1000000,
    WHOLE_SECONDS = 0x2000000,
    STICKY_TIMEOUTS = 0x4000000,
    ADDR_LIMIT_3GB = 0x8000000,
};

// if we ever have level 5 cache support
// change this to 57
#define __VIRTUAL_MASK_SHIFT	47 
#define TASK_SIZE_MAX	((1ULL << __VIRTUAL_MASK_SHIFT) - PAGE_SIZE)


// TODO: add to kernel information
// TODO: for now, let's just use the minimal value
#define DEFAULT_MAP_WINDOW	((1ULL << 47) - PAGE_SIZE)

// if x32 build
// PAGE_OFFSET
// else
#define IA32_PAGE_OFFSET(type)	((type) ? 0xc0000000 : 0xFFFFe000)
// endif

static size_t task_size_64bit(int full_addr_space)
{
    return full_addr_space ? TASK_SIZE_MAX : DEFAULT_MAP_WINDOW;
}

static void find_start_end(mm_struct_k mm, va_kernel_pointer_t addr, bool taskType, bool bits32,
    va_kernel_pointer_t *begin, va_kernel_pointer_t *end)
{

    *begin = mm_struct_get_mmap_legacy_base_size_t(mm);

    if (bits32)
    {
        *end = IA32_PAGE_OFFSET(taskType);
    }
    else
    {
        *end = task_size_64bit(addr > DEFAULT_MAP_WINDOW);
    }
}


#define STACK_GUARD_GAP 256UL << PAGE_SHIFT;

static inline size_t vm_start_gap(vm_area_struct_k vma)
{
    size_t vm_start = vm_area_struct_get_vm_start_size_t(vma);
    size_t vm_flags = vm_area_struct_get_vm_flags_size_t(vma);

    if (vm_flags & VM_GROWSDOWN) {
        vm_start -= STACK_GUARD_GAP;
        if (vm_start > vm_area_struct_get_vm_start_size_t(vma))
            vm_start = 0;
    }
    return vm_start;
}


bool RequestMappIngType(task_k task)
{
    return task_get_personality_uint32(task) & ADDR_LIMIT_3GB;
}

static va_kernel_pointer_t vm_unmapped_area(vm_unmapped_area_info * info)
{
    return unmapped_area(info);
}

va_kernel_pointer_t RequestUnmappedArea(mm_struct_k mm, bool type, va_kernel_pointer_t addr, size_t length, bool bits32)
{
    vm_area_struct_k vma;
    vm_unmapped_area_info info;
    va_kernel_pointer_t begin, end;

    if (!deprecated_mpx_check(addr, length, /*flags*/ 0))
        return addr;

    find_start_end(mm, addr, type, bits32, &begin, &end);

    if (length > end)
        return -1;

    if (addr) {
        addr = addr;
        vma = find_vma(mm, addr);
        if (end - length >= addr &&
            (!vma || addr + length <= vm_start_gap(vma)))
            return addr;
    }

    info.flags = VM_UNMAPPED_AREA_PROVIDED_MM;
    info.mm = mm;
    info.length = length;
    info.low_limit = begin;
    info.high_limit = end;
    info.align_mask = 0;
    info.align_offset = 0;

    return vm_unmapped_area(&info);
}
