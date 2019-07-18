/*
    Purpose: Generic process API built on top of linux
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/

#include <libos.hpp>

#include "OProcesses.hpp"
#include "OProcessHelpers.hpp"
#include "OProcessTransversal.hpp"

#include "OThreadImpl.hpp"

// Other parts of LibOS
#include "../FIO/OPath.hpp"
#include "Core/CPU/OThread.hpp"

task_k g_init_task;
//l_unsigned_long page_offset_base;
void InitProcesses()
{
    //page_offset_base = *(l_unsigned_long*)kallsyms_lookup_name("page_offset_base");
    g_init_task = kallsyms_lookup_name("init_task");
}

void ProcessesConvertPath(void * path, char * buf, size_t length)
{
    ODumbPointer<OLinuxPathImpl> tpath;
    error_t er;

    if (!path)
    {
        buf[0] = '\0';
        return;
    }
    
    er = OpenLinuxPath(OOutlivableRef<OLinuxPathImpl>(tpath), path);
    ASSERT(NO_ERROR(er), "OpenLinuxPath failed, 0x%zx", er);
    
    memset(buf, 0, length);
    tpath->ToString(buf, length);
}

OProcessImpl::OProcessImpl(task_k tsk)
{
    ProcessesTaskIncrementCounter(tsk);
    _tsk = tsk;
    _pid = ProcessesGetPid(tsk);

    InitModName();
    InitPaths();
    InitSec();
}

void OProcessImpl::InitSec()
{
    
}

void OProcessImpl::InitModName()
{
    memcpy(_name, task_get_comm(_tsk), THREAD_NAME_LEN);
    _name[strnlen(_name, sizeof(_name) - 1)] = 0;
}

void OProcessImpl::InitPaths()
{
#define FIX_NULLS(member)\
    member[strnlen(member, sizeof(member) - 1)] = 0;

    fs_struct_k fs;
    file_k file;

    file = get_task_exe_file(_tsk); //this is RCU safe unlike our old crap
    if (file)
    {
        ProcessesConvertPath(file_get_f_path(file), _path, sizeof(_path));
        fput(file);
    }

    ProcessesAcquireTaskFields(_tsk);
    fs = (fs_struct_k)task_get_fs_size_t(_tsk);
    if (fs)
    {
        ProcessesConvertPath(fs_struct_get_root(fs), _root, sizeof(_root));
        ProcessesConvertPath(fs_struct_get_pwd(fs), _working, sizeof(_working));
    }
    else
    {
        _root[0] = '\0';
        _working[0] = '\0';
    }
    ProcessesReleaseTaskFields(_tsk);

    FIX_NULLS(_root);
    FIX_NULLS(_working);
    FIX_NULLS(_path);
}

error_t OProcessImpl::GetProcessName(const char ** name)
{
    CHK_DEAD;

    if (!name)
        return kErrorIllegalBadArgument;

    *name = _name;
    return kStatusOkay;
}

error_t OProcessImpl::GetOSHandle(void ** handle)
{
    CHK_DEAD;

    if (!handle)
        return kErrorIllegalBadArgument;

    *handle = _tsk;
    return kStatusOkay;
}

error_t OProcessImpl::GetProcessId(uint_t * id)
{
    CHK_DEAD;

    if (!id)
        return kErrorIllegalBadArgument;

    *id = _pid;
    return kStatusOkay;
}

error_t OProcessImpl::GetModulePath(const char **path)
{
    CHK_DEAD;

    if (!path)
        return kErrorIllegalBadArgument;

    *path = _path;
    return kStatusOkay;
}

error_t OProcessImpl::GetDrive(const char **mnt)
{
    CHK_DEAD;

    if (!mnt)
        return kErrorIllegalBadArgument;

    *mnt = _root;
    return kStatusOkay;
}

error_t OProcessImpl::GetWorkingDirectory(const char **wd)
{
    CHK_DEAD;

    if (!wd)
        return kErrorIllegalBadArgument;

    *wd = _working;
    return kStatusOkay;
}


struct TempThreadCountData
{
    uint_t counter;
};

static bool ThreadCountCallback(const ThreadFoundEntry * thread, void * data)
{
    TempThreadCountData * priv = reinterpret_cast<TempThreadCountData *>(data);
    priv->counter++;
    return true;
}

uint_t OProcessImpl::GetThreadCount()
{
    TempThreadCountData temp = { 0 };
    LinuxTransverseThreadsInProcess(_tsk, ThreadCountCallback, &temp);
    return temp.counter;
}


struct TempThreadGetByIdData
{
    TempThreadGetByIdData(uint_t _search, OProcess * _parent) : search(_search), parent(_parent), err(kErrorProcessPidInvalid), thread(nullptr)
    {

    }

    error_t err;
    uint_t search;
    OProcessThread * thread;
    OProcess * parent;
};

static bool ThreadGetByIdCallback(const ThreadFoundEntry * thread, void * data)
{
    TempThreadGetByIdData * priv = reinterpret_cast<TempThreadGetByIdData *>(data);
    OProcessThreadImpl * proc;

    if (thread->threadId != priv->search)
        return true;

    proc = new OProcessThreadImpl(thread->task, thread->isProcess, priv->parent);
    
    priv->err = proc ? kStatusOkay : kErrorOutOfMemory;
    priv->thread = proc;

    return false;
}

error_t OProcessImpl::GetThreadById(uint_t id, const OOutlivableRef<OProcessThread> & thread)
{
    CHK_DEAD;

    TempThreadGetByIdData temp(id, this);
    LinuxTransverseThreadsInProcess(_tsk, ThreadGetByIdCallback, &temp);

    if (NO_ERROR(temp.err))
        thread.PassOwnership(temp.thread);

    return temp.err;
}


struct TempThreadIterationData
{
    ThreadIterator_cb callback;
    OProcessThreadImpl * thread;
    void * data;
    OProcess * parent;
};

static bool ThreadIterateCallback(const ThreadFoundEntry * thread, void * data)
{
    bool ret;
    TempThreadIterationData * priv = reinterpret_cast<TempThreadIterationData *>(data);

    new (priv->thread) OProcessThreadImpl(thread->task, thread->isProcess, priv->parent);

    ret = priv->callback((OProcessThread *)priv->thread, priv->data);

    priv->thread->Invalidate();

    memset(priv->thread, 0, sizeof(OProcessThreadImpl));
    return ret;
}

error_t OProcessImpl::IterateThreads(ThreadIterator_cb callback, void * ctx)
{
    CHK_DEAD;
    TempThreadIterationData temp;
    OProcessThreadImpl * proc;

    proc = (OProcessThreadImpl *)zalloc(sizeof(OProcessThreadImpl));
    if (!proc)
        return kErrorOutOfMemory;

    temp.thread   = proc;
    temp.parent   = this;
    temp.data     = ctx;
    temp.callback = callback;

    LinuxTransverseThreadsInProcess(_tsk, ThreadIterateCallback, &temp);

    free(proc);
    return kStatusOkay;
}

bool OProcessImpl::Is32Bits()
{
    return UtilityIsTask32Bit(_tsk);
}

error_t OProcessImpl::Terminate(bool force)
{
    CHK_DEAD;
    siginfo info;

    info.si_code = SI_KERNEL;
    info.si_errno = 0;
    info.si_signo = force ? SIGKILL : SIGTERM;
    info._kill._pid = 0;
    info._kill._uid = 0;

    send_sig_info(force ? SIGKILL : SIGTERM, &info, _tsk);
    return kStatusOkay; // assume ok for now
}

#include <Core/Memory/Linux/OLinuxMemory.hpp>

error_t OProcessImpl::AccessProcessMemory(user_addr_t address, void * buffer, size_t length, bool read)
{
    CHK_DEAD;
    uint32_t pages;
    page_k *page_array;
    user_addr_t start;
    l_long TODO;
    size_t remaining;
    size_t written;
    mm_struct_k  mm;
    size_t pg_offset;
    void * map;
    error_t ret;
    OLMemoryInterface * lm;
    OLPageEntryMeta protection;

    ret = GetLinuxMemoryInterface(lm);
    if (ERROR(ret))
        return ret;

    protection = lm->CreatePageEntry(OL_ACCESS_READ | OL_ACCESS_WRITE, kCacheNoCache);

    if (length == 0)
        return kErrorIllegalArgLength;

    if (!address)
        return kErrorIllegalBadArgument;

    if (!buffer)
        return kErrorIllegalBadArgument;

    pages      = (length / OS_PAGE_SIZE) + 1;
    page_array = (page_k *)zalloc(pages * sizeof(page_k));

    if (!page_array)
        return kErrorInternalError;

    mm = ProcessesAcquireMM_Read(_tsk);
    if (!mm)
        return kErrorInternalError;

    start = (user_addr_t)(size_t(address) & (OS_PAGE_MASK));

    if (OSThread == _tsk)
        TODO = get_user_pages_fast((l_unsigned_long)start, pages, read ? 0 : 1, page_array);
    else
        TODO = get_user_pages_remote(_tsk, mm, (l_unsigned_long)start, pages, read ? 0 : FOLL_WRITE, page_array, NULL, NULL);

    if (TODO <= 0)
    {
        ret = kERrorOutOfRange;
        goto exit;
    }

    // TODO: on x86_64 and i think arm64, we could use the linear kernel map
    // iirc arm64 vaddr = page->vaddr;
    map = vmap(page_array, pages, 0, protection.kprot);
    if (!map)
    {
        ret = kErrorInternalError;
        goto exit;
    }

    pg_offset = size_t(address) - size_t(start);
    if (read)
        memcpy(buffer, (void *)(size_t(map) + pg_offset), length);
    else
        memcpy((void *)(size_t(map) + pg_offset), buffer, length);

    vunmap(map);

exit:
    ProcessesReleaseMM_Read(mm);
    free(page_array);
    return ret;
}

error_t OProcessImpl::ReadProcessMemory(user_addr_t address, void * buffer, size_t length)
{
    return AccessProcessMemory(address, buffer, length, true);
}

error_t OProcessImpl::WriteProcessMemory(user_addr_t address, const void * buffer, size_t length)
{
    return AccessProcessMemory(address, (/*AHHHH i know this is safe, but it seems scary*/void *)buffer, length, false);
}

void OProcessImpl::InvalidateImp()
{
    ProcessesTaskDecrementCounter(_tsk);
}
