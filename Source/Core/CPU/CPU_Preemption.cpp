/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#include <xenus_lazy.h>
#include <libos.hpp>

#include <Core/CPU/OThread.hpp>

void ThreadingNoPreempt()
{
    preempt_disable();
    //preempt_count_add(1);
    //ThreadingMemoryFlush();
}

void ThreadingAllowPreempt()
{
    //ThreadingMemoryFlush();
    //preempt_count_sub(1);
    preempt_enable_reseched();
}
