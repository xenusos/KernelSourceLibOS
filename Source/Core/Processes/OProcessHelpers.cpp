/*
    Purpose: Generic process API built on top of linux
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/

#include <libos.hpp>
#include "OProcessHelpers.hpp"
#include <ITypes/IThreadStruct.hpp>
#include <ITypes/ITask.hpp>

void ProcessesMMIncrementCounter(mm_struct_k mm)
{
    _InterlockedIncrement((long *)mm_struct_get_mm_users(mm));
}

void ProcessesMMDecrementCounter(mm_struct_k mm)
{
    mmput(mm);
}

void ProcessesAcquireTaskFields(task_k tsk)
{
    _raw_spin_lock(task_get_alloc_lock(tsk));
}

void ProcessesReleaseTaskFields(task_k tsk)
{
    _raw_spin_unlock(task_get_alloc_lock(tsk));
}

void ProcessesTaskIncrementCounter(task_k  tsk)
{
    _InterlockedIncrement((long *)task_get_usage(tsk));
}

void ProcessesTaskDecrementCounter(task_k  tsk)
{
    long users;

    users = _InterlockedDecrement((long *)task_get_usage(tsk));

    if (users == 0)
        __put_task_struct(tsk);/*free task struct*/
}


mm_struct_k ProcessesAcquireMM(task_k tsk)
{
    mm_struct_k mm;
    rw_semaphore_k semaphore;

    ProcessesAcquireTaskFields(tsk);
    mm = (mm_struct_k)task_get_mm_size_t(tsk);
    
    if (!mm)
    {
        ProcessesReleaseTaskFields(tsk);
        return nullptr;
    }

    ProcessesMMIncrementCounter(mm);
    ProcessesReleaseTaskFields(tsk);
    return mm;
}

mm_struct_k ProcessesAcquireMM_Read(task_k tsk)
{
    mm_struct_k mm;

    mm = ProcessesAcquireMM(tsk);

    if (!mm)
        return nullptr;

    down_read((rw_semaphore_k)mm_struct_get_mmap_sem(mm));
    return mm;
}

mm_struct_k ProcessesAcquireMM_Write(task_k tsk)
{
    mm_struct_k mm;

    mm = ProcessesAcquireMM(tsk);

    if (!mm)
        return nullptr;

    down_write((rw_semaphore_k)mm_struct_get_mmap_sem(mm));
    return mm;
}

void ProcessesReleaseMM_Read(mm_struct_k mm)
{
    up_read((rw_semaphore_k)mm_struct_get_mmap_sem(mm));
    ProcessesMMDecrementCounter(mm);
}

void ProcessesReleaseMM_Write(mm_struct_k mm)
{
    up_write((rw_semaphore_k)mm_struct_get_mmap_sem(mm));
    ProcessesMMDecrementCounter(mm);
}

uint_t ProcessesGetPid(task_k tsk)
{
    return ITask(tsk).GetVarPID().GetUInt();
}

uint_t ProcessesGetTgid(task_k tsk)
{
    return ITask(tsk).GetVarTGID().GetUInt();
}

task_k ProcessesGetGroupLeader(task_k task)
{
    task_k leader = (task_k)task_get_group_leader_size_t(task);
    return leader ? leader : task;
}

task_k ProcessesGetProcess(task_k task)
{
    // GetGroupLeader(task) should equal the return of this function
    // https://elixir.bootlin.com/linux/v4.14.133/source/kernel/fork.c#L1840
    // I'm just trying to be safe
    return ProcessesGetPid(task) == ProcessesGetTgid(task) ? task : ProcessesGetGroupLeader(task);
}
