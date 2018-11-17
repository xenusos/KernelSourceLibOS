/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#include <xenus_lazy.h>
#include <libos.hpp>

#include <Core/CPU/OThread.hpp>

XENUS_BEGIN_C
extern void _ReadWriteBarrier(void);
extern void __faststorefence(void);
XENUS_END_C

void ThreadingMemoryFlush()
{
    _ReadWriteBarrier();
    __faststorefence();
}