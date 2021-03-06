/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#include <libos.hpp>
#include "OMemoryCoherency.hpp"

void CPU::Memory::ReadWriteBarrier()
{
    _ReadWriteBarrier();
    __faststorefence();
}
