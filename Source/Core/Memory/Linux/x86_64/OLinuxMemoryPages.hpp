/*
    Purpose: x86_64 linux memory interfaces
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#pragma once
#include <Core/Memory/Linux/OLinuxMemory.hpp>

extern PhysAllocationElem   * AllocateLinuxPages(OLPageLocation location, size_t cnt, bool user, bool contig, bool pfns, size_t uflags = 0);
extern void                   FreeLinuxPages(PhysAllocationElem * pages);
