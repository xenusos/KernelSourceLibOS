/*
    Purpose: x86_64 linux memory interfaces
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#include <libos.hpp>
#include "OLinuxStack.hpp"

// Do note that the following to functions do not imply how the stack grows or where the original stack pointer pointed to
// Start & End is merely indicative of the virtual range 
void * SafeStackGetStart(task_k tsk)
{
    return (void *)task_get_stack_size_t(tsk);
}

void * SafeStackGetEnd(task_k tsk)
{
    return (void *)(task_get_stack_size_t(tsk) + OS_THREAD_SIZE - 1);
}

bool SafeStackIsInRangeEx(task_k task, void * address, ssize_t length)
{
    size_t start;
    size_t end;
    size_t addr;
    size_t min;
    size_t max;

    start = (size_t)SafeStackGetStart(task);
    end   = (size_t)SafeStackGetEnd(task);

    addr  = (size_t)address;

    if (length > 0)
    {
        min = addr;
        max = size_t(ssize_t(min) + length);
    }
    else
    {
         max = addr;                          //min = address
         min = size_t(ssize_t(max) + length); //min =+ -length
    }

    ASSERT(min < max, "min isn't less than max in " __FUNCTION__);

    if (min < start)
        return false;

    if (max > end)
        return false;

    return true;
}

bool SafeStackIsInRange(void * address, ssize_t length)
{
    return SafeStackIsInRangeEx(OSThread, address, length);
}

bool   SafeStackCanAlloc(size_t length)
{
    size_t a;
    return SafeStackIsInRangeEx(OSThread, &a, 0 - length - 512);
}

size_t SafeStackGetApproxUsed()
{
    size_t a;
    return size_t(SafeStackGetEnd(OSThread)) - size_t(&a);
}

size_t SafeStackGetApproxFree()
{
    size_t a;
    return size_t(&a) - size_t(SafeStackGetStart(OSThread)); 
}

void   SafeStackCheckState()
{
    size_t a;

    if (!SafeStackIsInRange(&a, 1))
        panic("Stack Buffer Overflow Detected!");
}
