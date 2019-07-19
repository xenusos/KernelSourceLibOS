/*
    Purpose: Generic process API built on top of linux
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/

#include <libos.hpp>
#include "OProcesses.hpp"
#include "OProcessHelpers.hpp"
#include "OProcessTransversal.hpp"

struct TempProcessGetByIdData
{
    TempProcessGetByIdData(uint_t _search) : search(_search), err(kErrorProcessPidInvalid), thread(nullptr)
    {

    }

    error_t err;
    uint_t search;
    OProcess * thread;
};

static bool ProcessGetByIdCallback(const ThreadFoundEntry * thread, void * data)
{
    TempProcessGetByIdData * priv = reinterpret_cast<TempProcessGetByIdData *>(data);
    OProcessImpl * proc;

    if (!thread->isProcess)
        return true;

    if (thread->threadId != priv->search)
        return true;

    proc = new OProcessImpl(thread->task);

    priv->err    = proc ? kStatusOkay : kErrorOutOfMemory;
    priv->thread = proc;

    return false;
}

LIBLINUX_SYM error_t GetProcessById(uint_t id, const OOutlivableRef<OProcess> process)
{
    TempProcessGetByIdData temp(id);

    LinuxTransverseAll(ProcessGetByIdCallback, &temp);

    if (NO_ERROR(temp.err))
        process.PassOwnership(temp.thread);

    return temp.err;
}


struct TempProcessGetParentByIdData
{
    TempProcessGetParentByIdData(uint_t _search) : search(_search), err(kErrorProcessPidInvalid), thread(nullptr)
    {

    }

    error_t err;
    uint_t search;
    OProcess * thread;
};

static bool ProcessParentGetByIdCallback(const ThreadFoundEntry * thread, void * data)
{
    TempProcessGetByIdData * priv = reinterpret_cast<TempProcessGetByIdData *>(data);
    OProcessImpl * proc;

    if (!thread->isProcess)
        return true;

    if (thread->threadId != priv->search)
        return true;

    if (!thread->spawner)
    {
        priv->err    = kErrorProcessPidInvalid;
        priv->thread = nullptr;
        return false;
    }

    proc = new OProcessImpl(thread->spawner);

    priv->err = proc ? kStatusOkay : kErrorOutOfMemory;
    priv->thread = proc;

    return false;
}

LIBLINUX_SYM error_t GetProcessParentById(uint_t id, const OOutlivableRef<OProcess> process)
{
    TempProcessGetByIdData temp(id);

    LinuxTransverseAll(ProcessParentGetByIdCallback, &temp);

    if (NO_ERROR(temp.err))
        process.PassOwnership(temp.thread);

    return temp.err;
}

LIBLINUX_SYM error_t GetProcessByCurrent(const OOutlivableRef<OProcess> process)
{
    OProcess * proc;
    task_k me;
    task_k leader;

    me     = OSThread;
    leader = ProcessesGetProcess(me);

    if (!process.PassOwnership(new OProcessImpl(leader ? leader : me)))
        return kErrorOutOfMemory;
    
    return kStatusOkay;
}

struct TempProcessIterationData
{
    ProcessIterator_cb callback;
    OProcessImpl * proc;
    void * data;
};

static bool ProcessIterateCallback(const ThreadFoundEntry * thread, void * data)
{
    bool ret;
    TempProcessIterationData * priv = reinterpret_cast<TempProcessIterationData *>(data);

    if (!thread->isProcess)
        return true;

    // placement new - allocate new process instance in preallocated buffer
    new (priv->proc) OProcessImpl(thread->task);
    
    // callback user functionm
    ret = priv->callback((OProcess *)priv->proc, priv->data);
    
    // reset process
    // lets hope that the overhead of memory allocation is less than memset
    priv->proc->Invalidate();
    memset(priv->proc, 0, sizeof(OProcessImpl));
    return ret;
}

static error_t GetProcessesCommon(bool root, ProcessIterator_cb callback, void * data)
{
    TempProcessIterationData temp;
    OProcessImpl * proc;

    proc = (OProcessImpl *)zalloc(sizeof(OProcessImpl));
    if (!proc)
        return kErrorOutOfMemory;

    temp.proc = proc;
    temp.data = data;
    temp.callback = callback;

    if (root)
        LinuxTransverseThreadsInProcess(g_init_task, ProcessIterateCallback, &temp);
    else
        LinuxTransverseThreadsEntireProcess(g_init_task, ProcessIterateCallback, &temp);

    free(proc);
    return kStatusOkay;
}

LIBLINUX_SYM error_t GetProcessesByAll(ProcessIterator_cb callback, void * data)
{
    return GetProcessesCommon(false, callback, data);
}

LIBLINUX_SYM error_t GetProcessesAtRootLevel(ProcessIterator_cb callback, void * data)
{
    return GetProcessesCommon(true, callback, data);
}

LIBLINUX_SYM uint_t  GetProcessCurrentId()
{
    return ProcessesGetTgid(OSThread);
}

LIBLINUX_SYM uint_t  GetProcessCurrentTid()
{
    return ProcessesGetPid(OSThread);
}
