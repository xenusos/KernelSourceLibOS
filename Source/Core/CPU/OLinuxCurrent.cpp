/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#include <libos.hpp>
#include "OLinuxCurrent.hpp"
#include <Core/Utilities/OThreadUtilities.hpp>

using namespace CPU::Current;

void LinuxCurrent::SetCPUAffinity(cpumask mask)
{

}

void LinuxCurrent::GetCPUAffinity(cpumask & mask)
{

}

void LinuxCurrent::PushAddressLimit()
{
    ASSERT(!_addr_pushed, "illegal address limit state");
    _addr_pushed = true;
    _addr_limit = _task_i.SwapAddressLimit(0 /* DS_KERNEL */);
}

void LinuxCurrent::PopAddressLimit()
{
    _task_i.SetAddressLimitUnsafe(_addr_limit);
    _addr_pushed = false;
}

void LinuxCurrent::SnoozeMS(uint32_t ms)
{
    msleep(ms);
}

void LinuxCurrent::SnoozeNanoRange(uint64_t u, uint64_t mu)
{
    usleep_range(u, mu);
}

l_int LinuxCurrent::GetRealPRIO()
{
    return  _task_i.GetPRIO();
}

l_int LinuxCurrent::GetPrio()
{
    return  _task_i.GetStaticPRIO();
}

l_uint LinuxCurrent::GetCPU()
{
    return xenus_util_get_cpuid();
}

void LinuxCurrent::StopPreemption()
{
    Utilities::Tasks::DisablePreemption();
}

void LinuxCurrent::StartPreemption()
{
    Utilities::Tasks::AllowPreempt();
}

int32_t LinuxCurrent::GetNice()
{
    return PRIO_TO_NICE(_task_i.GetStaticPRIO());
}

void LinuxCurrent::SetNice(int32_t ahh)
{
    set_user_nice(_task, ahh);
}
