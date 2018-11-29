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

task_k init_task;
//l_unsigned_long page_offset_base;
void InitProcesses()
{
    //page_offset_base = *(l_unsigned_long*)kallsyms_lookup_name("page_offset_base");
    init_task = kallsyms_lookup_name("init_task");
}

void ProcessesMMLock(mm_struct_k mm)
{
    _InterlockedIncrement((long *)mm_struct_get_mm_users(mm));
}

void ProcessesMMUnlock(mm_struct_k mm)
{
    mmput(mm);
}

void ProcessesLockTask(task_k tsk)
{
    _raw_spin_lock(task_get_alloc_lock(tsk));
}

void ProcessesUnlockTask(task_k tsk)
{
    _raw_spin_unlock(task_get_alloc_lock(tsk));
}

void ProcessesTaskStructInc(task_k  tsk)
{
    _InterlockedIncrement((long *)task_get_usage(tsk));
}

void ProcessesTaskStructDec(task_k  tsk)
{
    bool should_exit;

    should_exit = _InterlockedDecrement((long *)task_get_usage(tsk)) == 0;

    if (should_exit)
        __put_task_struct(tsk);
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
    ORetardPtr<OLinuxPathImpl> tpath;
    error_t er;
    if (!path)
    {
        buf[0] = '\0';
        return;
    }
    ASSERT((er = OpenLinuxPath(OOutlivableRef<OLinuxPathImpl>(tpath), path)) == kStatusOkay, "OpenLinuxPath failed, %lli", er);
    memset(buf, 0, length);
    tpath->ToString(buf, length);
}

OProcessThreadImpl::OProcessThreadImpl(task_k tsk, OProcess * process)
{
    ProcessesTaskStructInc(tsk);
    _tsk = tsk;
    _id = ProcessesGetPid(tsk);
    _process = process;

    memcpy(_name, task_get_comm(_tsk), THREAD_NAME_LEN);
}

void OProcessThreadImpl::InvalidateImp()
{
    ProcessesTaskStructDec(_tsk);
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
    ProcessesTaskStructInc(tsk);
    _threads_mutex = mutex_create();
    _tsk = tsk;
    _pid = ProcessesGetPid(tsk);

    _threads = nullptr;

    _threads_mutex = mutex_create();
    ASSERT(_threads_mutex, "unrecoverable memory error in constructor of OProcessImpl - i know this constructor doesn't follow the normal design...");

    InitModName();
    InitPaths();
    InitSec();
}

void OProcessImpl::InitSec()
{
    _lvl = kProcessUsrGeneric; //TODO
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

    ProcessesLockTask(_tsk);
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
    ProcessesUnlockTask(_tsk);
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

error_t OProcessImpl::GetGenericSecLevel(ProcessSecurityLevel_e * sec)
{
    CHK_DEAD;
    if (!sec)
        return kErrorIllegalBadArgument;
    *sec = _lvl;
    return kStatusOkay;
}

error_t OProcessImpl::UpdateThreadCache()
{
    CHK_DEAD;
    task_k cur;
    task_k srt;
    error_t er;

    er = kStatusOkay;

    mutex_lock(_threads_mutex);

    if (!_threads)
    {
        if (!(_threads = DYN_LIST_CREATE(OProcessThreadImpl *)))
        {
            mutex_unlock(_threads_mutex);
            return kErrorOutOfMemory;
        }
    }

    dyn_list_reset(_threads);

    if (!_threads)
    {
        er = kErrorOutOfMemory;
        goto exit;
    }

    cur = srt = _tsk;
    if (cur)
    {
        do
        {
            list_head * head;
            OProcess ** thread;
            OProcess * inst;

            if (ERROR(er = dyn_list_append(_threads, (void **)&thread)))
                goto exit;

            inst = (OProcess *)new OProcessThreadImpl(cur, this); // todo: recursively add child groups

            if (!inst)
            {
                er = kErrorOutOfMemory;
                goto exit;
            }

            *thread = inst;

            head = (list_head *)task_get_thread_group(cur);
            cur = (task_k)((uint64_t)(head->next) - (uint64_t)task_get_thread_group(NULL));
        } while (cur != srt);
    }

exit:
    mutex_unlock(_threads_mutex);
    return er;
}

uint_t OProcessImpl::GetThreadCount()
{
    CHK_DEAD;
    size_t cnt;
    dyn_list_entries(_threads, &cnt);
    return cnt;
}

error_t OProcessImpl::IterateThreads(ThreadIterator_cb callback, void * ctx)
{
    CHK_DEAD;
    error_t er;
    size_t cnt;

    er = kStatusOkay;
    if (!_threads)
        UpdateThreadCache();

    mutex_lock(_threads_mutex);

    if (ERROR(er = dyn_list_entries(_threads, &cnt)))
        goto exit;

    for (size_t i = 0; i < cnt; i++)
    {
        OProcess ** thread;

        if (ERROR(er = dyn_list_get_by_index(_threads, i, (void **)&thread)))
            goto exit;

        callback(*thread, ctx);
    }


exit:
    mutex_unlock(_threads_mutex);

    return er;
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
    rw_semaphore_k semaphore;
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
    OLPageEntry protection;

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

    pages = (length / kernel_information.LINUX_PAGE_SIZE) + 1;
    page_array = (page_k *)zalloc(pages * sizeof(page_k));

    if (!page_array)
        return kErrorInternalError;

    ProcessesLockTask(_tsk);
    mm = (mm_struct_k)task_get_mm_size_t(_tsk);

    if (!mm)
    {
        ProcessesUnlockTask(_tsk);
        return kErrorInternalError;
    }

    ProcessesMMLock(mm);
    ProcessesUnlockTask(_tsk);

    start = (user_addr_t)(size_t(address) & (kernel_information.LINUX_PAGE_MASK));

    semaphore = (rw_semaphore_k)mm_struct_get_mmap_sem(mm); // &mm->mmap_sem
    down_read(semaphore);									// lock semaphore

    TODO = get_user_pages_remote(_tsk, mm, (l_unsigned_long)start, pages, FOLL_FORCE, page_array, NULL, NULL); // WHAT DOES THIS RETURN? 

    if (!(map = vmap(page_array, pages, 0, protection.prot)))
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
    ProcessesMMUnlock(mm);
    up_read(semaphore);									 // unlock semaphore
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
    ProcessesTaskStructDec(_tsk);
    dyn_list_destory(_threads);
}

error_t GetProcessById(uint_t id, const OOutlivableRef<OProcess> process)
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
            cur = (task_k)((uint64_t)(head->next) - (uint64_t)task_get_tasks(NULL)); // next/priv is volatile, READ_ONCE just dereferences a volatilevalue
                                                                                     // hopefully this is legal enough
                                                                                     // especially considering x86/AMD64 guarantees atomicity of type read/writes on sizeof(type) boundaries
                                                                                     // no idea how linux makes this thread safe... dont really care
        } while (cur != srt);														 // cbfa to read all the RCU related patents and headers
    }
    return kErrorProcessPidInvalid;
}

error_t GetProcessByCurrent(const OOutlivableRef<OProcess> process)
{
    return GetProcessById(ProcessesGetTgid(OSThread), process);
}

error_t GetProcessesByAll(ProcessIterator_cb callback, void * data)
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
            cur = (task_k)((uint64_t)(head->next) - (uint64_t)task_get_tasks(NULL));
        } while (cur != srt);
    }
    return kStatusOkay;
}
