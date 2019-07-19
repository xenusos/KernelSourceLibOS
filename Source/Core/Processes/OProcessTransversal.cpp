/*
    Purpose: Generic process API built on top of linux
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt) (See License.txt)
*/
#include <libos.hpp>
#include "OProcessTransversal.hpp"
#include "OProcessHelpers.hpp"
#include "OProcesses.hpp"
#include <ITypes/IThreadStruct.hpp>
#include <ITypes/ITask.hpp>

#include "../../Utils/RCU.hpp"

static bool KernelTransverseThreads(task_k process, task_k parent, ThreadFoundCallback_f callback, void * data, bool onlyScanProcess = false);

static bool KernelIsThreadGroup(task_k task)
{
    return ProcessesGetTgid(task) == ProcessesGetPid(task);
}

static bool KernelFoundTask(task_k task, task_k parent, ThreadFoundCallback_f callback, void * data)
{
    ThreadFoundEntry thread;

    thread.task          = task;
    thread.isProcess     = KernelIsThreadGroup(task);
    thread.threadId      = ProcessesGetPid(task);
    thread.tgid          = ProcessesGetTgid(task);
    thread.realProcessId = thread.isProcess ? parent ? ProcessesGetPid(parent) : 0 : thread.threadId;
    thread.realProcess   = thread.isProcess ? parent ? parent  : nullptr : task;
    thread.spawner       = parent;

    return callback(&thread, data);
}

static task_k KernelNextThreadGroup(task_k cur)
{
    list_head * head;
    head = (list_head *)task_get_thread_group(cur);
    return (task_k)(uint64_t(head->next) - uint64_t(task_get_thread_group(NULL)));
}

static bool KernelTransverseThreadGroup(task_k groupThread, ThreadFoundCallback_f callback, void * data)
{
    for (task_k cur = KernelNextThreadGroup(groupThread);
        cur != groupThread;
        cur = KernelNextThreadGroup(cur))
    {
        if (!KernelFoundTask(cur, groupThread, callback, data))
            return false;
    }
    return true;
}

static bool KernelTransverseSiblings(task_k process, ThreadFoundCallback_f callback, void * data, bool onlyScanProcess) // sub processes a/k/a !CLONE_THREAD 
{
    volatile list_head * cur;
    list_head_k srt;

    cur = srt = (list_head_k)task_get_children(process);

    for (cur = srt->next; cur != srt; cur = cur->next)
    {
        task_k thread;
        task_k parent;

        thread = (task_k)(uint64_t(cur) - uint64_t(task_get_sibling(NULL)));
        parent = (task_k)task_get_parent_size_t(thread);

        // https://elixir.bootlin.com/linux/v4.14.133/source/kernel/fork.c#L1939
        ASSERT(KernelIsThreadGroup(thread), "Sibling is not a thread leader!");

        if (!onlyScanProcess)
        {
            if (!KernelTransverseThreads(thread, process, callback, data))
                return false;
        }
        else
        {
            if (!KernelFoundTask(thread, process, callback, data))
                return false;
        }
    }

    return true;
}

static bool KernelTransverseThreads(task_k process, task_k parent, ThreadFoundCallback_f callback, void * data, bool onlyScanProcess)
{
    if (!KernelFoundTask(process, parent, callback, data))
        return false;
    
    if (!KernelTransverseThreadGroup(process, callback, data))
        return false;

    if (!KernelTransverseSiblings(process, callback, data, onlyScanProcess))
        return false;

    return true;
}

bool LinuxTransverseAll(ThreadFoundCallback_f callback, void * data)
{
    bool ret;
    RCU::ReadLock();
    ret = KernelTransverseThreads(g_init_task, nullptr, callback, data);
    RCU::ReadUnlock();
    return ret;
}

bool LinuxTransverseThreadsInProcess(task_k task, ThreadFoundCallback_f callback, void * data)
{
    bool ret;
    RCU::ReadLock();
    ret = KernelTransverseThreads(task, nullptr, callback, data, true);
    RCU::ReadUnlock();
    return ret;
}

bool LinuxTransverseThreadsEntireProcess(task_k task, ThreadFoundCallback_f callback, void * data)
{
    bool ret;
    RCU::ReadLock();
    ret = KernelTransverseThreads(task, nullptr, callback, data, false);
    RCU::ReadUnlock();
    return ret;
}
