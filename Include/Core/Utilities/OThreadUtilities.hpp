/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#pragma once

namespace Utilities
{
    namespace Tasks
    {
        LIBLINUX_SYM void DisablePreemption();
        LIBLINUX_SYM void AllowPreempt();

        LIBLINUX_SYM bool IsTask32Bit(task_k handle);
    }
}
