/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#include <libos.hpp>
#include "OThreadUtilities.hpp"

void Utilities::Tasks::DisablePreemption()
{
    preempt_disable();
    //preempt_count_add(1);
    //ThreadingMemoryFlush();
}

void Utilities::Tasks::AllowPreempt()
{
    //ThreadingMemoryFlush();
    //preempt_count_sub(1);
    preempt_enable_reseched();
}

bool Utilities::Tasks::IsTask32Bit(task_k handle)
{
    uint32_t flags = thread_info_get_flags_uint32(task_get_thread_info(handle));
    return flags & 29;
    // TIF_IA32	- x86_32 instructions
    // TIF_ADDR32 - 32bit size_t
    // TIF_X32 - native x86_32 console 
}
