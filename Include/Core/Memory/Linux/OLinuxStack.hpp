/*
    Purpose: Linux specific low-level memory operations
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#pragma once

LIBLINUX_SYM void * SafeStackGetEnd        (task_k tsk);
LIBLINUX_SYM void * SafeStackGetStart      (task_k tsk);
LIBLINUX_SYM bool   SafeStackIsInRangeEx   (task_k task, void * address, ssize_t length);
LIBLINUX_SYM bool   SafeStackIsInRange     (void * address, ssize_t length);
LIBLINUX_SYM bool   SafeStackCanAlloc      (size_t length);

LIBLINUX_SYM size_t SafeStackGetApproxUsed();
LIBLINUX_SYM size_t SafeStackGetApproxFree();
LIBLINUX_SYM void   SafeStackCheckState();