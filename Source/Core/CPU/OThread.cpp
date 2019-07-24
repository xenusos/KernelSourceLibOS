/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/  
#include <libos.hpp>
#include "OThread.hpp"
#include "../Processes/OProcesses.hpp"
#include <Core/Synchronization/OSpinlock.hpp>
#include <Core/Synchronization/OSemaphore.hpp>
#include <ITypes/IThreadStruct.hpp>
#include <ITypes/ITask.hpp>

static void * thread_dealloc_mutex;
static void * thread_chain_mutex;
static chain_p thread_handle_chain;
static chain_p thread_ep_chain;
static /*atomic*/ long closing_threads;

typedef struct ThreadPrivData_s
{
    CPU::Threading::OThreadEP_t entrypoint;
    void * data;
    const char * name;
} ThreadPrivData_t, *ThreadPrivData_p;

struct 
{
    mutex_k mutex;
    Synchronization::OCountingSemaphore * semaphore;
    OThreadImp * instance;
} sync_thread_create;

OThreadImp::OThreadImp(task_k tsk, uint32_t id, const char * name, const void * data)
{
    this->_tsk  = tsk;
    this->_id   = id;
    this->_data = data;
    memset(this->_name, 0, sizeof(this->_name));

    if (name)
        memcpy(this->_name, name, MIN(strlen(name), sizeof(this->_name) - 1));

    this->_try_kill = false;
}

error_t OThreadImp::GetExitCode(int64_t & code)
{
    CHK_DEAD;
    code = _exit_code;
    return _tsk == nullptr ? XENUS_ERROR_GENERIC_FAILURE : XENUS_OKAY;
}

error_t OThreadImp::IsAlive(bool & ret)
{
    CHK_DEAD;
    ret = _tsk ? true : false;
    return XENUS_OKAY;
}

error_t OThreadImp::IsRunning(bool & out)
{
    CHK_DEAD;
    out = _tsk ? true : false; //TODO: probably running
    return XENUS_OKAY;
}

error_t OThreadImp::IsMurderable(bool & out)
{
    CHK_DEAD;

    if (!_tsk)
    {
        out = false;
        return kStatusOkay;
    }

    if (_try_kill)
    {
        out = false;
        return kStatusOkay;
    }

    out = true; //TODO: probably murderable
    return kStatusOkay;
}

error_t OThreadImp::GetPOSIXNice(int32_t & nice)
{
    CHK_DEAD;

    Lock();

    if (!_tsk)
    {
        Unlock();
        return kErrorTaskNull;
    }

    nice = PRIO_TO_NICE(ITask(_tsk).GetStaticPRIO());
    Unlock();

    return kStatusOkay;
}

error_t OThreadImp::SetPOSIXNice(int32_t nice)
{
    CHK_DEAD;

    Lock();

    if (!_tsk)
    {
        Unlock();
        return kErrorTaskNull;
    }

    set_user_nice(_tsk, nice);
    Unlock();

    return kStatusOkay;
}

error_t OThreadImp::GetName(const char *& str)
{
    CHK_DEAD;
    str = this->_name;
    return kStatusOkay;
}

error_t OThreadImp::IsFloatingHandle(bool & ret)
{
    CHK_DEAD;
    ret = _tsk ?  false : true;
    return kStatusOkay;
}

error_t OThreadImp::GetOSHandle(void *& handle)
{
    CHK_DEAD;
    handle = _tsk;
    return _tsk == nullptr ? kErrorGenericFailure : kStatusOkay;
}


error_t OThreadImp::GetThreadId(uint32_t & id)
{
    CHK_DEAD;
    id = this->_id;
    return _tsk == nullptr ? kErrorGenericFailure : kStatusOkay;
}

void * OThreadImp::GetData()
{
    return nullptr;
}

void OThreadImp::SignalDead(long exitcode)
{
    Lock();
    _tsk = nullptr;
    _exit_code = exitcode;
    Unlock();
}

error_t OThreadImp::TryMurder(long exitcode)
{
    CHK_DEAD;

    Lock();

    if (!_tsk)
    {
        Unlock();
        return kErrorTaskNull;
    }

    if (this->_try_kill)
    {
        Unlock();
        return kStatusAlreadyExiting;
    }

    this->_try_kill = true;

    *_death_code = exitcode;
    _InterlockedIncrement(&closing_threads);
    *_death_signal = _id;

    // poke the thread to ensure our post context switch handler is called within the next year or so...
    wake_up_process(this->_tsk); 

    Unlock();
    return XENUS_STATUS_NOT_ACCURATE_ASSUME_OKAY;
}

void OThreadImp::Lock()
{
    _task_holder.Lock();
}


void OThreadImp::Unlock()
{
    _task_holder.Unlock();
}

void OThreadImp::InvalidateImp()
{
    mutex_lock(thread_dealloc_mutex);
    chain_deallocate_search(thread_handle_chain, this->_id);
    mutex_unlock(thread_dealloc_mutex);
}

long ** OThreadImp::DeathSignal()
{
    return &_death_signal;
}

long ** OThreadImp::DeathCode()
{
    return &_death_code;
}

void InitThreading()
{
    error_t err;

    err = chain_allocate(&thread_handle_chain);
    if (ERROR(err))
        panic("Couldn't create thread handle tracking chain");

    err = chain_allocate(&thread_ep_chain);
    if (ERROR(err))
        panic("Couldn't create thread ep tracking chain");

    sync_thread_create.mutex = mutex_create();
    ASSERT(sync_thread_create.mutex, "couldn't allocate mutex");

    err = CreateCountingSemaphore(0, sync_thread_create.semaphore);
    ASSERT(NO_ERROR(err), "couldn't create semaphore");

    thread_chain_mutex   = mutex_create();
    thread_dealloc_mutex = mutex_create();

    ASSERT(thread_chain_mutex, "couldn't allocate mutex");
    ASSERT(thread_dealloc_mutex, "couldn't allocate mutex");
}

static void ThreadExitNtfyEP(uint32_t pid, long exitcode)
{
    error_t ret;
    CPU::Threading::OThreadEP_t * ep_tracker;
    link_p link;

    ret = chain_get(thread_ep_chain, pid, &link, (void **)&ep_tracker);
    if (ERROR(ret))
        return;

    // ntfy thread exit
    {
        CPU::Threading::ThreadMsg_t msg;
        msg.type = CPU::Threading::kMsgThreadExit;
        msg.exit.thread_id = thread_geti();
        msg.exit.code = exitcode;
        (*ep_tracker)(&msg);
    }

    // remove ep from chain
    {
        chain_deallocate_handle(link);
    }
}

static void ThreadExitNtfyObject(uint32_t pid, long exitcode)
{
    error_t ret;
    link_p link;
    OThreadImp ** thread_handle;

    mutex_lock(thread_dealloc_mutex);
    ret = chain_get(thread_handle_chain, pid, &link, (void **)&thread_handle);
    if (NO_ERROR(ret))
    {
        (*thread_handle)->SignalDead(exitcode);
    }
    mutex_unlock(thread_dealloc_mutex);
}

static void RuntimeThreadExit(long exitcode)
{
    error_t ret;
    link_p link;
    uint32_t pid;

    pid =  thread_geti();

    mutex_lock(thread_chain_mutex);
    {
        // ep hackery
        ThreadExitNtfyEP(pid, exitcode);

        // try notify othreadimpl that its controlling a dead handle, if not already nuked from a dumb pointer.
        ThreadExitNtfyObject(pid, exitcode);
    }
    mutex_unlock(thread_chain_mutex);
}

static void RuntimeThreadPostContextSwitch()
{
    error_t err;
    volatile long * exitCode;
    volatile long * exitSignal;

    if (!closing_threads)
        return;

    err = _thread_tls_get(TLS_TYPE_XGLOBAL, 1, NULL, (void **)&exitSignal);
    if (err == kErrorBSTNodeNotFound)
        return;

    ASSERT(NO_ERROR(err), "Couldn't get task exit code TLS entry (error: " PRINTF_ERROR ")", err);

    if (!*exitSignal)
        return;

    *exitSignal = false;

    err = _thread_tls_get(TLS_TYPE_XGLOBAL, 2, NULL, (void **)&exitCode);
    ASSERT(NO_ERROR(err), "Couldn't get task exit code TLS entry (error: " PRINTF_ERROR ")", err);

    _InterlockedDecrement(&closing_threads);

    // Stop linux whining 
    preempt_enable();

    // Night
    do_exit((int32_t)*exitCode);
}

static void ThreadEPAllocateTLSEntries(OThreadImp * instance)
{
    error_t ret;

    ret = _thread_tls_allocate(TLS_TYPE_XGLOBAL, 1, sizeof(long), NULL, (void **)instance->DeathSignal());
    ASSERT(NO_ERROR(ret), "couldn't create thread death signal. error code: " PRINTF_ERROR, ret);

    ret = _thread_tls_allocate(TLS_TYPE_XGLOBAL, 2, sizeof(long), NULL, (void **)instance->DeathCode());
    ASSERT(NO_ERROR(ret), "couldn't create thread death code. error code: " PRINTF_ERROR, ret);
}

static void ThreadEPHandleChains(uint32_t pid, OThreadImp * instance, CPU::Threading::OThreadEP_t ep)
{
    error_t ret;
    OThreadImp ** handle;
    CPU::Threading::OThreadEP_t * ep_tracker;

    mutex_lock(thread_chain_mutex);

    ret = chain_allocate_link(thread_handle_chain, pid, sizeof(size_t), nullptr, nullptr, (void **)&handle);
    ASSERT(NO_ERROR(ret), "couldn't create thread link. error code: " PRINTF_ERROR, ret);
    *handle = instance;

    ret = chain_allocate_link(thread_ep_chain, pid, sizeof(size_t), nullptr, nullptr, (void **)&ep_tracker);
    ASSERT(NO_ERROR(ret), "couldn't create thread link. error code: " PRINTF_ERROR, ret);
    *ep_tracker = ep;

    mutex_unlock(thread_chain_mutex);
}

static void ThreadEPInitExitHandler()
{
    thread_exit_cb_t * cb_arr;
    int cb_cnt;
    int i = 0;

    threading_get_exit_callbacks(&cb_arr, &cb_cnt);
    
    while (cb_arr[i])
    {
        i++;
        if (i >= cb_cnt)
        {
            panic("couldn't install thread exit callback for libos runtime");
        }
    }

    cb_arr[i] = RuntimeThreadExit;
}

static void ThreadEPAddContextSwitchHandler()
{
    // allow the murdering of our threads
    // this is one of many things that would make linux developers very very angry, if they knew people were doing sane stuff with their nasty ass kernel

    thread_post_context_switch_hook(RuntimeThreadPostContextSwitch);
}

static int RuntimeThreadEP(void * data)
{
    OThreadImp * instance;
    task_k task;
    ThreadPrivData_p priv;
    CPU::Threading::OThreadEP_t ep_stub;
    void * ep_data;
    const char * th_name;
    uint32_t pid;
    int exitcode;
    uint32_t idc;
    uint32_t idfc;

    task    = OSThread;
    pid     = thread_geti();

    priv    = (ThreadPrivData_p)(data);

    ep_stub = priv->entrypoint;
    ep_data = priv->data;
    th_name = priv->name;

    free((void *)priv);

    // allocate thread
    instance = new OThreadImp(task, pid, th_name, ep_data);
    ASSERT(instance, "couldn't allocate OThread instance");

    ThreadEPHandleChains(pid, instance, ep_stub);
    ThreadEPAllocateTLSEntries(instance);
    ThreadEPInitExitHandler();

    ThreadEPAddContextSwitchHandler();
    
    // nasty ass hack
    //HackProcessesOThreadHook();

    // ntfy thread create
    {
        CPU::Threading::ThreadMsg_t msg;
        
        msg.type = CPU::Threading::kMsgThreadCreate;
        msg.create.data      = reinterpret_cast<void *>(ep_data);
        msg.create.thread_id = pid;
        msg.create.thread    = instance;

        ep_stub(&msg);
    }

    sync_thread_create.instance = instance;
    sync_thread_create.semaphore->Trigger(1, idc, idfc);

    // ntfy thread start
    {
        CPU::Threading::ThreadMsg_t msg;
        
        msg.type       = CPU::Threading::kMsgThreadStart;
        msg.start.data = reinterpret_cast<void *>(ep_data);
        msg.start.code = 0;
        ep_stub(&msg);

        exitcode = msg.start.code;
    }

    return exitcode;
}

error_t CPU::Threading::SpawnOThread(const OOutlivableRef<CPU::Threading::OThread> & thread, CPU::Threading::OThreadEP_t entrypoint, const char * name, void * data)
{
    error_t err;
    task_k task;
    ThreadPrivData_p priv;
    
    priv = (ThreadPrivData_p)malloc(sizeof(ThreadPrivData_t));
    ASSERT(priv, "couldn't allocate temp thread storage");

    priv->entrypoint = entrypoint;
    priv->data       = data;
    priv->name       = name;

    mutex_lock(sync_thread_create.mutex);
    sync_thread_create.instance = nullptr;

    err = thread_create(&task, RuntimeThreadEP, priv, name, true);
    if (ERROR(err))
    {
        mutex_unlock(sync_thread_create.mutex);
        return err;
    }
    
    sync_thread_create.semaphore->Wait();

    thread.PassOwnership(sync_thread_create.instance);
    mutex_unlock(sync_thread_create.mutex);
    return kStatusOkay;
}
