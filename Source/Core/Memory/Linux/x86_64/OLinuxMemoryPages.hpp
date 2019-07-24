/*
    Purpose: x86_64 linux memory interfaces
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#pragma once
#include <Core/Memory/Linux/OLinuxMemory.hpp>

extern Memory::PhysAllocationElem   * AllocateLinuxPages(Memory::OLPageLocation location, size_t cnt, bool user, bool contig, bool pfns, size_t uflags = 0);
extern void                           FreeLinuxPages(Memory::PhysAllocationElem * pages);
