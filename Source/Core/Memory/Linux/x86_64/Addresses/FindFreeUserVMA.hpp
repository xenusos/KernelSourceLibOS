/*
    Purpose: x86_64 linux memory interfaces
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#pragma once

typedef size_t va_kernel_pointer_t;

extern bool RequestMappIngType(task_k task);
// note: all parameters must be aligned
extern va_kernel_pointer_t  RequestUnmappedArea(mm_struct_k mm, bool type, va_kernel_pointer_t addr, size_t length, bool bits32);
