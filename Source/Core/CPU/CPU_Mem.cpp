/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#include <libos.hpp>
#include <Core/CPU/OThread.hpp>

void ThreadingMemoryFlush()
{
    _ReadWriteBarrier();
    __faststorefence();
}
