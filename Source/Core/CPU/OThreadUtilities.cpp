/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson
*/  
#include <libos.hpp>
#include "OThreadUtilities.hpp"


bool UtilityIsTask32Bit(task_k handle)
{
    uint32_t flags = thread_info_get_flags_uint32(task_get_thread_info(handle));
    return flags & 17; // TIF_IA32	
}
