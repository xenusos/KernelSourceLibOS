/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson
*/  
#include <libos.hpp>

#include "OThread.hpp"

#include <ITypes/IThreadStruct.hpp>
#include <ITypes/ITask.hpp>

#include "../Processes/OProcesses.hpp"

#include <Core/CPU/OSpinlock.hpp>
#include <Core/CPU/OSemaphore.hpp>

static void * thread_dealloc_mutex;
static void * thread_chain_mutex;
static chain_p thread_handle_chain;
static chain_p thread_ep_chain;

typedef struct ThreadPrivData_s
{
    OThreadEP_t entrypoint;
    void * data;
    const char * name;
} ThreadPrivData_t, *ThreadPrivData_p;

struct 
{
    los_spinlock_t writing;
    los_spinlock_t mutex;
    long/*atomic*/ readable;
    uint32_t pid;
    int32_t exitcode;
} sync_thread_death;

struct 
{
    mutex_k mutex;
    OCountingSemaphore * semaphore;

    OThreadImp * instance;
} sync_thread_create;

LinuxCurrent::LinuxCurrent() 
{
    _task = OSThread;
    _addr_pushed = false;
    _addr_limit = 0;
    _task_i = new ITask(_task); 
}

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
    _addr_limit = _task_i->SwapAddressLimit(0 /* DS_KERNEL */);
}

void LinuxCurrent::PopAddressLimit()
{
    _task_i->SetAddressLimitUnsafe(_addr_limit);
    _addr_pushed = false;
}

void LinuxCurrent::SnoozeMS(uint64_t ms)
{
    msleep(ms);
}

void LinuxCurrent::SnoozeNanoRange(uint64_t u, uint64_t mu)
{
    usleep_range(u, mu);
}

l_int LinuxCurrent::GetRealPRIO()
{
    return  _task_i->GetPRIO();
}

l_int LinuxCurrent::GetPrio()
{
    return  _task_i->GetStaticPRIO();
}

l_uint LinuxCurrent::GetCPU()
{
    return xenus_util_get_cpuid();
}

void LinuxCurrent::StopPreemption()
{
    ThreadingNoPreempt();
}

void LinuxCurrent::StartPreemption()
{
    ThreadingAllowPreempt();
}

int32_t LinuxCurrent::GetNice()
{
    return PRIO_TO_NICE(_task_i->GetStaticPRIO());
}

void LinuxCurrent::SetNice(int32_t ahh)
{
    set_user_nice(_task, ahh);
}

OThreadImp::OThreadImp(task_k tsk, uint32_t id, const char * name, const void * data)
{
    this->_tsk  = tsk;
    this->_id   = id;
    this->_data = data;
    memset(this->_name, 0, sizeof(this->_name));
    if (name)
        memcpy(this->_name, name, MIN(strlen(name), sizeof(this->_name) - 1));
    this->_try_kill = false;
    SpinLock_Init(&this->_task_holder);
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

    SpinLock_Lock(&sync_thread_death.mutex);
    {
        // lazy spinlock based semaphore
        SpinLock_Lock(&sync_thread_death.writing);

        // populate job info
        sync_thread_death.exitcode = exitcode;
        sync_thread_death.pid = this->_id;
        sync_thread_death.readable = 1;
    }
    SpinLock_Unlock(&sync_thread_death.mutex);

    // poke the thread to ensure our post context switch handler is called within the next year or so...
    wake_up_process(this->_tsk); 

    Unlock();
    return XENUS_STATUS_NOT_ACCURATE_ASSUME_OKAY;
}

void OThreadImp::Lock()
{
    SpinLock_Lock(&this->_task_holder);
}


void OThreadImp::Unlock()
{
    SpinLock_Unlock(&this->_task_holder);
}

void OThreadImp::InvalidateImp()
{
    mutex_lock(thread_dealloc_mutex);
    chain_deallocate_search(thread_handle_chain, this->_id);
    mutex_unlock(thread_dealloc_mutex);
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

    SpinLock_Init(&sync_thread_death.mutex);
    SpinLock_Init(&sync_thread_death.writing);
    sync_thread_death.readable = 0;

    sync_thread_create.mutex = mutex_create();
    ASSERT(sync_thread_create.mutex, "couldn't allocate mutex");

    err = CreateCountingSemaphore(0, sync_thread_create.semaphore);
    ASSERT(NO_ERROR(err), "couldn't create semaphore");

    thread_chain_mutex   = mutex_create();
    thread_dealloc_mutex = mutex_create();

    ASSERT(thread_chain_mutex, "couldn't allocate mutex");
    ASSERT(thread_dealloc_mutex, "couldn't allocate mutex");
}

static void RuntimeThreadExit(long exitcode)
{
    OThreadImp ** thread_handle;
    OThreadEP_t * ep_tracker;
    error_t ret;
    link_p link;
    uint32_t pid;

    pid =  thread_geti();

    mutex_lock(thread_chain_mutex);
    {
        // ep hackery
        {
            if (NO_ERROR(ret = chain_get(thread_ep_chain, pid, &link, (void **)&ep_tracker)))
            {
                // ntfy thread exit
                {
                    ThreadMsg_t msg;
                    msg.type = kMsgThreadExit;
                    msg.exit.thread_id = thread_geti();
                    msg.exit.code = exitcode;
                    (*ep_tracker)(&msg);
                }
                // remove ep from chain
                {
                    chain_deallocate_handle(link);
                }
            }
        }

        // try notify othreadimpl that its controlling a dead handle, if not already nuked from a dumb pointer.
        {
            mutex_lock(thread_dealloc_mutex);
            if (NO_ERROR(ret = chain_get(thread_handle_chain, pid, &link, (void **)&thread_handle)))
            {
                (*thread_handle)->SignalDead(exitcode);
            }
            mutex_unlock(thread_dealloc_mutex);
        }
    }
    mutex_unlock(thread_chain_mutex);
}

static void RuntimeThreadPostContextSwitch()
{
    bool exit;
    uint32_t pid;
    int32_t exitcode;

    pid  = thread_geti();
    exit = false;

    SpinLock_Lock(&sync_thread_death.mutex);
    {
        if (SpinLock_IsLocked(&sync_thread_death.writing))
        {
            while (!sync_thread_death.readable)
            {
                SPINLOOP_PROCYIELD();
            }

            if (sync_thread_death.pid == pid)
            {
                // yes, we are the chosen one!
                exitcode = sync_thread_death.exitcode;
                exit     = true;

                // reset global state
                sync_thread_death.readable = 0;
                SpinLock_Unlock(&sync_thread_death.writing);
            }
        }
    }
    SpinLock_Unlock(&sync_thread_death.mutex);
   
    if (!exit)
        return;

    // Prevent linux from whining and to prevent linux from sleeping during a no-preempt state
    ThreadingAllowPreempt();   
    
    // Night
    do_exit(exitcode);
}

static int RuntimeThreadEP(void * data)
{
    thread_exit_cb_t * cb_arr;
    OThreadImp * instance;
    int cb_cnt;
    task_k task;
    ThreadPrivData_p priv;
    OThreadEP_t ep_stub;
    const void * ep_data;
    const char * th_name;
    uint32_t pid;
    int exitcode;

    task    = OSThread;
    pid     = thread_geti();

    priv    = (ThreadPrivData_p)(data);

    ep_stub = priv->entrypoint;
    ep_data = priv->data;
    th_name = priv->name;

    free((void *)priv);

    // allocate thread
    {
        instance = new OThreadImp(task, pid, th_name, ep_data);
        ASSERT(instance, "couldn't allocate OThread instance");
    }

    // allocate links
    {
        error_t ret;
        OThreadImp ** handle;
        OThreadEP_t * ep_tracker;

        mutex_lock(thread_chain_mutex);

        ASSERT(NO_ERROR(ret = chain_allocate_link(thread_handle_chain, pid, sizeof(size_t), nullptr, nullptr, (void **)&handle)), "couldn't create thread link. error code: %lli", ret);
        *handle = instance;
        
        ASSERT(NO_ERROR(ret = chain_allocate_link(thread_ep_chain, pid, sizeof(size_t), nullptr, nullptr, (void **)&ep_tracker)), "couldn't create thread link. error code: %lli", ret);
        *ep_tracker = ep_stub;
        
        mutex_unlock(thread_chain_mutex);
    }

    // install linux kernel exit thread callback
    {
        threading_get_exit_callbacks(&cb_arr, &cb_cnt);
        int i = 0;
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

    // allow the murdering of our threads
    // this is one of many things that would make linux developers very very angry, if they knew people were doing sane stuff with their nasty ass kernel
    thread_post_context_switch_hook(RuntimeThreadPostContextSwitch);

    // nasty ass hack
    //HackProcessesOThreadHook();

    // ntfy thread create
    {
        ThreadMsg_t msg;
        
        msg.type = kMsgThreadCreate;
        msg.create.data      = (void *)ep_data;
        msg.create.thread_id = pid;
        msg.create.thread    = instance;

        ep_stub(&msg);
    }


    sync_thread_create.instance = instance;
    uint32_t idc;
    uint32_t idfc;
    sync_thread_create.semaphore->Trigger(1, idc, idfc);

    // ntfy thread start
    {
        ThreadMsg_t msg;
        
        msg.type       = kMsgThreadStart;
        msg.start.data = (void *)ep_data;
        msg.start.code = 0;
        ep_stub(&msg);

        exitcode = msg.start.code;
    }

    return exitcode;
}

error_t SpawnOThread(const OOutlivableRef<OThread> & thread, OThreadEP_t entrypoint, const char * name, void * data)
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
