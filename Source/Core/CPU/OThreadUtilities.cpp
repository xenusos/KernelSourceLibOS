/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/  
#include <libos.hpp>
#include "OThreadUtilities.hpp"


bool UtilityIsTask32Bit(task_k handle)
{
    uint32_t flags = thread_info_get_flags_uint32(task_get_thread_info(handle));
    return flags & 29;
    
    // TIF_IA32	- x86_32 instructions
    // TIF_ADDR32 - 32bit size_t
    // TIF_X32 - native x86_32 console 
}
