/*
    Purpose: x86_64 linux memory interfaces
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#pragma once
#include <Core/Memory/Linux/OLinuxStack.hpp>

LIBLINUX_SYM void * Memory::Stack::GetEnd(task_k tsk);
LIBLINUX_SYM void * Memory::Stack::GetStart(task_k tsk);
LIBLINUX_SYM bool   Memory::Stack::IsInRangeEx(task_k task, void * address, ssize_t length);
LIBLINUX_SYM bool   Memory::Stack::IsInRange(void * address, ssize_t length);
LIBLINUX_SYM bool   Memory::Stack::CanAlloc(size_t length);

LIBLINUX_SYM size_t Memory::Stack::GetApproxUsed();
LIBLINUX_SYM size_t Memory::Stack::GetApproxFree();
LIBLINUX_SYM void   Memory::Stack::CheckState();
