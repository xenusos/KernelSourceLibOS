/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#pragma once

struct ThreadFoundEntry
{
    task_k task;
    bool isProcess;
    union
    {
        uint_t threadId;
        uint_t pid;
    };
    uint_t tgid;
    uint_t realProcessId;
    task_k realProcess;
};

typedef bool(*ThreadFoundCallback_f)(const ThreadFoundEntry * thread, void * data);

extern bool LinuxTransverseAll(ThreadFoundCallback_f callback, void * data);
extern bool LinuxTransverseThreadsInProcess(task_k task, ThreadFoundCallback_f callback, void * data);
extern bool LinuxTransverseThreadsEntireProcess(task_k task, ThreadFoundCallback_f callback, void * data);
