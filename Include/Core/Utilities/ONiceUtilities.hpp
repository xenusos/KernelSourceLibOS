/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#pragma once

namespace Utilities
{
    namespace Nice
    {
        LIBLINUX_SYM int8_t  NiceToKPRIO(uint8_t windows);
        LIBLINUX_SYM uint8_t KPTRIOToNice(int8_t nice);
    }
}
