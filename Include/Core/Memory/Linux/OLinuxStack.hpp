/*
    Purpose: Linux specific low-level memory operations
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#pragma once

namespace Memory
{
    namespace Stack
    {
        LIBLINUX_SYM void * GetEnd(task_k tsk);
        LIBLINUX_SYM void * GetStart(task_k tsk);
        LIBLINUX_SYM bool   IsInRangeEx(task_k task, void * address, ssize_t length);
        LIBLINUX_SYM bool   IsInRange(void * address, ssize_t length);
        LIBLINUX_SYM bool   CanAlloc(size_t length);

        LIBLINUX_SYM size_t GetApproxUsed();
        LIBLINUX_SYM size_t GetApproxFree();
        LIBLINUX_SYM void   CheckState();
    }
}
