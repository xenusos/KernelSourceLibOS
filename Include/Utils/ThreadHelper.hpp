/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/  
#pragma once

namespace ThreadHelpers
{
    LIBLINUX_SYM uint32_t  GetAddressLimit();
    LIBLINUX_SYM uint32_t  SwapAddressLimit(uint32_t fs);
    LIBLINUX_SYM void  SetAddressLimitUnsafe(uint32_t fs);

    LIBLINUX_ASM void *  GetIP();
}
