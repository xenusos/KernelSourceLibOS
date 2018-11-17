/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson
*/  
#include <xenus_lazy.h>
#include <libtypes.hpp>
#include <libos.hpp>

#include "OThread.hpp"

#include <ITypes/IThreadStruct.hpp>
#include <ITypes/ITask.hpp>

#include "../Processes/OProcesses.hpp"

static void * thread_dealloc_mutex;
static void * thread_chain_mutex;
static chain_p thread_handle_chain;
static chain_p thread_ep_chain;

typedef struct ThreadPrivData_s
{
    OThreadEP_t entrypoint;
    const void * data;
    const char * name;
} ThreadPrivData_t, *ThreadPrivData_p;

struct 
{
    los_spinlock_t working;
    uint32_t pid;
    int32_t exitcode;
} sync_thread_death;

struct 
{
    los_spinlock_t working;
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
	return 0;
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
}

error_t OThreadImp::GetExitCode(int64_t & code)
{
    CHK_DEAD
	code = _exit_code;
	return _tsk == nullptr ? XENUS_ERROR_GENERIC_FAILURE : XENUS_OKAY;
}

error_t OThreadImp::IsAlive(bool & ret)
{
    CHK_DEAD
	ret = _tsk ? true : false;
	return XENUS_OKAY;
}

error_t OThreadImp::IsRunning(bool & out)
{
    CHK_DEAD
    out = _tsk ? true : false; //TODO: probably running
    return XENUS_OKAY;
}

error_t OThreadImp::IsMurderable(bool & out)
{
    CHK_DEAD

    if (!_tsk)
    {
        out = false;
        return XENUS_OKAY;
    }

    if (_try_kill)
    {
        out = false;
        return XENUS_OKAY;
    }

    out = true; //TODO: probably murderable
	return XENUS_OKAY;
}

error_t OThreadImp::GetPOSIXNice(int32_t & nice)
{
    CHK_DEAD

    if (!_tsk)
        return XENUS_ERROR_TASK_NULL;

    nice = PRIO_TO_NICE(ITask(_tsk).GetStaticPRIO());
    return XENUS_OKAY;
}

error_t OThreadImp::SetPOSIXNice(int32_t nice)
{
    CHK_DEAD

    if (!_tsk)
        return XENUS_ERROR_TASK_NULL;

    set_user_nice(_tsk, nice);
    return XENUS_OKAY;
}

error_t OThreadImp::GetName(const char *& str)
{
    CHK_DEAD
	str = this->_name;
	return XENUS_OKAY;
}

error_t OThreadImp::IsFloatingHandle(bool & ret)
{
    CHK_DEAD
	ret = _tsk ?  false : true;
	return XENUS_OKAY;
}

error_t OThreadImp::GetOSHandle(void *& handle)
{
    CHK_DEAD
	handle = _tsk;
	return _tsk == nullptr ? XENUS_ERROR_GENERIC_FAILURE : XENUS_OKAY;
}


error_t OThreadImp::GetThreadId(uint32_t & id)
{
    CHK_DEAD
	id = this->_id;
	return _tsk == nullptr ? XENUS_ERROR_GENERIC_FAILURE : XENUS_OKAY;
}

void * OThreadImp::GetData()
{
	return nullptr;
}

void OThreadImp::SignalDead(long exitcode)
{
	_tsk = nullptr;
	_exit_code = exitcode;
}

error_t OThreadImp::TryMurder(long exitcode)
{
    CHK_DEAD
	error_t ret = XENUS_STATUS_NOT_ACCURATE_ASSUME_OKAY;

	if (!_tsk)
		return XENUS_OKAY;

	this->_wanted_exit_code = exitcode;
	this->_try_kill = true;


    SpinLock_Lock(&sync_thread_death.working);

    sync_thread_death.exitcode = exitcode;
    sync_thread_death.pid = this->_id; //must be set last
	return ret;
}

void OThreadImp::InvaildateImp()
{
	mutex_lock(thread_dealloc_mutex);
	chain_deallocate_search(thread_handle_chain, this->_id);
	mutex_unlock(thread_dealloc_mutex);
}

void InitThreading()
{
	if (ERROR(chain_allocate(&thread_handle_chain)))
		panic("Couldn't create thread handle tracking chain");

	if (ERROR(chain_allocate(&thread_ep_chain)))
		panic("Couldn't create thread ep tracking chain");

    SpinLock_Init(&sync_thread_death.working);
    SpinLock_Init(&sync_thread_create.working);

	thread_chain_mutex   = mutex_create();
    thread_dealloc_mutex = mutex_create();

    ASSERT(thread_chain_mutex, "couldn't allocate mutex");
    ASSERT(thread_dealloc_mutex, "couldn't allocate mutex");
}


void RuntimeThreadExit(long exitcode)
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

void RuntimeThreadPostContextSwitch()
{
    error_t ret;
    link_p link;
    int32_t exitcode;
    uint32_t pid;

    pid = thread_geti();

    if (!sync_thread_death.working)
        return;

    if (sync_thread_death.pid != pid)
        return;

    exitcode = sync_thread_death.exitcode;
    sync_thread_death.pid = 0; // must be nulled
    SpinLock_Unlock(&sync_thread_death.working);

    ThreadingAllowPreempt();   // this is to prevent linux from whining and to prevent linux from sleeping during a no-preempt state
    
    do_exit(exitcode);
}

int RuntimeThreadEP(void * data)
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
    HackProcessesOThreadHook();

	// ntfy thread create
	{
		ThreadMsg_t msg;
		msg.type = kMsgThreadCreate;
		msg.create.data = (void *)ep_data;
		msg.create.thread = instance;
		ep_stub(&msg);
	}


	sync_thread_create.instance = instance; // see: hack_thread_instance

	// ntfy thread start
	{
		ThreadMsg_t msg;
		msg.type = kMsgThreadStart;
		msg.start.data = (void *)ep_data;
		msg.start.code = 0;
		ep_stub(&msg);
		exitcode = msg.start.code;
	}

    return exitcode;
}

error_t SpawnOThread(const OOutlivableRef<OThread> & thread, OThreadEP_t entrypoint, const char * name, const void * data)
{
	error_t err;
	task_k task;
    ThreadPrivData_p priv;
	
	priv = (ThreadPrivData_p)malloc(sizeof(ThreadPrivData_t));
	ASSERT(priv, "couldn't allocate temp thread storage");

    //TODAY:
	priv->entrypoint = entrypoint;
	priv->data       = data;
	priv->name       = name;

    SpinLock_Lock(&sync_thread_create.working);
    sync_thread_create.instance = nullptr;

	if (ERROR(err = thread_create(&task, RuntimeThreadEP, priv, name, true)))
	{
		ASSERT(!sync_thread_create.instance, "current thread instance should be null");
		return err;
	}
	
	while (!sync_thread_create.instance)
		SPINLOOP_SLEEP();
    
	thread.PassOwnership(sync_thread_create.instance);
    sync_thread_create.instance = nullptr;

    SpinLock_Unlock(&sync_thread_create.working);
	return kStatusOkay;
}