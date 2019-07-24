/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#include <libos.hpp>
#include "RCU.hpp"

void RCU::ReadLock()
{
    // no compiler warning
    // no dep map support
    __rcu_read_lock();
}

void RCU::ReadUnlock()
{
    // no compiler warning
    // no dep map support
    __rcu_read_unlock();
}
