/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#include <libos.hpp>
#include "ODECriticalSection.hpp"

static mutex_k mutex;

void EnterDECriticalSection()
{
    mutex_lock(mutex);
}

void LeaveDECriticalSection()
{
    mutex_unlock(mutex);
}

void InitDECriticalSection()
{
    mutex = mutex_create();
    ASSERT(mutex, "couldn't allocate mutex");
}
