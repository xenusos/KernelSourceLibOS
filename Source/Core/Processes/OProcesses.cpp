/*
    Purpose: Generic process API built on top of linux
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/

#include <libos.hpp>

#include "OProcesses.hpp"

// LibOSTypes
#include <ITypes/IThreadStruct.hpp>
#include <ITypes/ITask.hpp>

// Other parts of LibOS
#include "../FIO/OPath.hpp"
#include "../../Utils/RCU.hpp"
#include "Core/CPU/OThread.hpp"

task_k init_task;
//l_unsigned_long page_offset_base;
void InitProcesses()
{
    //page_offset_base = *(l_unsigned_long*)kallsyms_lookup_name("page_offset_base");
    init_task = kallsyms_lookup_name("init_task");
}

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

OProcessThreadImpl::OProcessThreadImpl(task_k tsk, OProcess * process)
{
    ProcessesTaskIncrementCounter(tsk);
    _tsk = tsk;
    _id = ProcessesGetPid(tsk);
    _process = process;

    memcpy(_name, task_get_comm(_tsk), THREAD_NAME_LEN);
}

void OProcessThreadImpl::InvalidateImp()
{
    ProcessesTaskDecrementCounter(_tsk);
}

error_t OProcessThreadImpl::GetThreadName(const char ** name)
{
    if (!name)
        return kErrorIllegalBadArgument;
    *name = _name;
    return kStatusOkay;
}

error_t OProcessThreadImpl::GetOSHandle(void ** handle)
{
    if (!handle)
        return kErrorIllegalBadArgument;
    *handle = _tsk;
    return kStatusOkay;
}

error_t OProcessThreadImpl::GetId(uint_t * id)
{
    if (!id)
        return kErrorIllegalBadArgument;
    *id = _id;
    return kStatusOkay;
}

error_t OProcessThreadImpl::GetParent(OUncontrollableRef<OProcess> parent)
{
    parent.SetObject(_process);
    return kStatusOkay;
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
}

void OProcessImpl::InitPaths()
{
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

uint_t OProcessImpl::GetThreadCount()
{
    uint_t cnt;
    task_k cur;
    task_k srt;

    RCU::ReadLock();

    cnt = 0;
    cur = srt = _tsk;
    if (cur)
    {
        do
        {
            list_head * head;

            cnt++;
           
            head = (list_head *)task_get_thread_group(cur);
            cur = (task_k)(uint64_t(head->next) - uint64_t(task_get_thread_group(NULL)));
        } while (cur != srt);
    }

    RCU::ReadUnlock();
    return cnt;
}

error_t OProcessImpl::GetThreadById(uint_t id, const OOutlivableRef<OProcessThread> & thread)
{
    CHK_DEAD;
    task_k cur;
    task_k srt;
    error_t er = kErrorProcessPidInvalid;
    OProcessThread * proc = nullptr;

    RCU::ReadLock();

    cur = srt = _tsk;
    if (cur)
    {
        do
        {
            list_head * head;

            if (ProcessesGetPid(cur) == id)
            {
                proc = new OProcessThreadImpl(cur, this);
                if (!proc)
                {
                    er = kErrorOutOfMemory;
                    goto exit;
                }

                er = kStatusOkay;
                goto exit;
            }


            head = (list_head *)task_get_thread_group(cur);
            cur  = (task_k)(uint64_t(head->next) - uint64_t(task_get_thread_group(NULL)));
        } while (cur != srt);
    }

exit:
    RCU::ReadUnlock();

    if (NO_ERROR(er))
        thread.PassOwnership(proc);

    return er;
}

error_t OProcessImpl::IterateThreads(ThreadIterator_cb callback, void * ctx)
{
    CHK_DEAD;
    task_k cur;
    task_k srt;
    error_t er = kStatusOkay;
    OProcessThread * proc;

    proc = (OProcessThread *)zalloc(sizeof(OProcessThreadImpl *));
    if (!proc)
        return kErrorOutOfMemory;

    RCU::ReadLock();

    cur = srt = _tsk;
    if (cur)
    {
        do
        {
            bool shouldRet;
            list_head * head;

            new (proc) OProcessThreadImpl(cur, this);

            shouldRet = !callback(proc, ctx);

            proc->Destory();
            memset(proc, 0, sizeof(OProcessThreadImpl *));

            head = (list_head *)task_get_thread_group(cur);
            cur  = (task_k)(uint64_t(head->next) - uint64_t(task_get_thread_group(NULL)));
        } while (cur != srt);
    }

    RCU::ReadUnlock();
    free(proc);
    return er;
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

    ret = kStatusOkay;

    if (ERROR(ret = GetLinuxMemoryInterface(lm)))
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
    if (!(map = vmap(page_array, pages, 0, protection.kprot)))
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

error_t _GetProcessById(uint_t id, const OOutlivableRef<OProcess> process)
{
    task_k cur;
    task_k srt;

    cur = srt = init_task;
    if (cur)
    {
        do
        {
            list_head * head;

            if ((id == ProcessesGetPid(cur)) && (id == ProcessesGetTgid(cur)))
            {
                OProcess * proc;

                proc = new OProcessImpl(cur);
                if (!proc)
                    return kErrorOutOfMemory;

                process.PassOwnership(proc);
                return kStatusOkay;
            }

            head = (list_head *)task_get_tasks(cur);
            cur  = (task_k)(uint64_t(head->next) - uint64_t(task_get_tasks(NULL)));
        } while (cur != srt);
    }
    return kErrorProcessPidInvalid;
}

LIBLINUX_SYM error_t GetProcessById(uint_t id, const OOutlivableRef<OProcess> process)
{
    error_t ret;
    RCU::ReadLock();
    ret = _GetProcessById(id, process);
    RCU::ReadUnlock();
    return ret;
}

LIBLINUX_SYM error_t GetProcessByCurrent(const OOutlivableRef<OProcess> process)
{
    OProcess * proc;
    task_k me;
    task_k leader;

    me     = OSThread;
    leader = (task_k)task_get_group_leader_size_t(me);

    proc = new OProcessImpl(leader ? leader : me);
    if (!process.PassOwnership(proc))
        return kErrorOutOfMemory;
    
    return kStatusOkay;
}

error_t _GetProcessesByAll(ProcessIterator_cb callback, void * data)
{
    task_k cur;
    task_k srt;

    if (!callback)
        return kErrorIllegalBadArgument;

    cur = srt = init_task;
    if (cur)
    {
        do
        {
            OProcess * proc;
            list_head * head;

            if (ProcessesGetTgid(cur) == ProcessesGetPid(cur))
            {
                proc = new OProcessImpl(cur);

                if (!proc)
                    return kErrorOutOfMemory;

                callback(proc, data);
                proc->Destory();
            }

            head = (list_head *)task_get_tasks(cur);
            cur  = (task_k)(uint64_t(head->next) - uint64_t(task_get_tasks(NULL)));
        } while (cur != srt);
    }
    return kStatusOkay;
}

LIBLINUX_SYM error_t GetProcessesByAll(ProcessIterator_cb callback, void * data)
{
    error_t ret;
    RCU::ReadLock();
    ret = _GetProcessesByAll(callback, data);
    RCU::ReadUnlock();
    return ret;
}

LIBLINUX_SYM uint_t  GetProcessCurrentId()
{
    return ProcessesGetTgid(OSThread);
}

LIBLINUX_SYM uint_t  GetProcessCurrentTid()
{
    return ProcessesGetPid(OSThread);
}
