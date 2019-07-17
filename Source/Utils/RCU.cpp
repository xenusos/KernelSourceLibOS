/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#include <libos.hpp>
#include "RCU.hpp"

void __rcu_read_lock()
{
    static sysv_fptr_t function;
    if (!function)
        function = (sysv_fptr_t)kallsyms_lookup_name("__rcu_read_lock");
    ASSERT(function, "no RCU symbol");
    ez_linux_caller(function, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

void __rcu_read_unlock()
{
    static sysv_fptr_t function;
    if (!function)
        function = (sysv_fptr_t)kallsyms_lookup_name("__rcu_read_unlock");
    ASSERT(function, "no RCU symbol");
    ez_linux_caller(function, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

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