/*
    Purpose: A Windows-APC style thread preemption within the Linux kernel [depends on latest xenus linux kernel patch]
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#include <libos.hpp>
#include "ODEThread.hpp"
#include "ODEProcess.hpp"
#include "ODeferredExecution.hpp"
#include "../../Memory/Linux/OLinuxMemory.hpp"
#include "../../Processes/OProcesses.hpp"

struct linux_thread_info // TODO: portable structs. NEVER TRUST MSVC and GCC TO AGREE
{
    l_unsignedlong flags;
    u32      state;
    atomic_t swap_bool;
    pt_regs  previous_user;
    pt_regs  next_user;
};

static linux_thread_info * GetInfoForTask(task_k tsk)
{
    return (linux_thread_info *)task_get_thread_info(tsk);
}

ODEImplPIDThread::ODEImplPIDThread(ODEImplProcess * parent)
{
    _proc = parent;
}

ODEImplPIDThread::~ODEImplPIDThread()
{
    if (_workPending)
        dyn_list_destory(_workPending);
}

error_t ODEImplPIDThread::Init()
{
    error_t err;

    err = AllocateStack();
    if (ERROR(err))
        return err;

    err = AllocatePendingWork();
    if (ERROR(err))
        return err;

    return kStatusOkay;
}

void ODEImplPIDThread::UpdatePidHandle(task_k task)
{
    _task = task;
}

error_t ODEImplPIDThread::AppendWork(ODEWorkHandler * handler)
{
    error_t err;
    size_t count;
    ODEWorkHandler ** entry;

    err = dyn_list_append(_workPending, (void **)&entry);
    if (ERROR(err))
        return err;

    *entry = handler;

    err = dyn_list_entries(_workPending, &count);
    ASSERT(NO_ERROR(err), "Error: 0x%zx", err);

    if (count == 1)
        PreemptExecutionForWork(handler, OSThread != _task /* we're going to return to userspace sooner or later, if this == requested*/);

    return kStatusOkay;
}

void ODEImplPIDThread::NtfyJobFinished(size_t ret)
{
    bool hasNext;
    ODEWorkHandler * next = nullptr;
    ODEWorkHandler * cur  = nullptr;
    pt_regs usrState;

    usrState = GetInfoForTask(_task)->previous_user;

    PopCompletedTask(cur, hasNext, next);
    cur->Hit(ret);

    if (hasNext)
    {
        SaveState(&usrState);
        PreemptExecutionForWork(next, false);
        return;
    }
    
    if (_userState.hasPreviousTask)
    {
        _userState.hasPreviousTask = false;
        PreemptExecution(&_userState.restore, false);
    }
    else
    {
        PreemptExecution(&usrState, false);
    }
}

void ODEImplPIDThread::SaveState(pt_regs_p state)
{
    if (_userState.hasPreviousTask)
        return;

    _userState.hasPreviousTask = true;
    _userState.restore = *state;
}

DEStack * ODEImplPIDThread::GetStack()
{
    return &_stack;
}

error_t ODEImplPIDThread::AllocateStack()
{
    error_t err;
    OLVirtualAddressSpace * krnVas;
    ODumbPointer<OLVirtualAddressSpace> usrVas;
    OPtr<OLMemoryAllocation> usrAlloc;
    OPtr<OLMemoryAllocation> krnAlloc;

    err = g_memory_interface->GetKernelAddressSpace(OUncontrollableRef<OLVirtualAddressSpace>(krnVas));
    if (ERROR(err))
    {
        LogPrint(kLogError, "Couldn't obtain kernel address space interface, error 0x%zx", err);
        return err;
    }

    err = g_memory_interface->GetUserAddressSpace(_task, OOutlivableRef<OLVirtualAddressSpace>(usrVas));
    if (ERROR(err))
    {
        LogPrint(kLogError, "Couldn't obtain user address space interface, error 0x%zx", err);
        return err;
    }

    err = krnVas->NewDescriptor(0, APC_STACK_PAGES, OOutlivableRef<OLMemoryAllocation>(krnAlloc));
    if (ERROR(err))
    {
        LogPrint(kLogError, "Couldn't allocate descriptor, error 0x%zx", err);
        return err;
    }

    err = usrVas->NewDescriptor(0, APC_STACK_PAGES, OOutlivableRef<OLMemoryAllocation>(usrAlloc));
    if (ERROR(err))
    {
        LogPrint(kLogError, "Couldn't allocate descriptor, error 0x%zx", err);
        krnAlloc->Destory();
        return err;
    }

    _stack.pages = usrVas->AllocatePages(kPageNormal, APC_STACK_PAGES, false, OL_PAGE_ZERO);
    if (!_stack.pages)
    {
        krnAlloc->Destory();
        usrAlloc->Destory();
        return kErrorOutOfMemory;
    }

    for (size_t i = 0; i < APC_STACK_PAGES; i++)
    {
        OLPageEntry entry;

        entry.meta = g_memory_interface->CreatePageEntry(OL_ACCESS_READ | OL_ACCESS_WRITE, kCacheNoCache);
        entry.type = kPageEntryByPage;
        entry.page = _stack.pages[i];

        err = krnAlloc->PageInsert(i, entry);
        if (ERROR(err))
        {
            LogPrint(kLogError, "Couldn't insert page into kernel address space 0x%zx", err);
            goto error;
        }

        err = usrAlloc->PageInsert(i, entry);
        if (ERROR(err))
        {
            LogPrint(kLogError, "Couldn't insert page into user address space 0x%zx", err);
            goto error;
        }
    }
    
    _stack.user.allocation   = usrAlloc;
    _stack.user.start        = usrAlloc->GetStart();
    _stack.user.end          = usrAlloc->GetEnd();

    _stack.kernel.allocation = krnAlloc;
    _stack.kernel.start      = krnAlloc->GetStart();
    _stack.kernel.end        = krnAlloc->GetEnd();
    return kStatusOkay;

error:
    usrVas->FreePages(_stack.pages);
    krnAlloc->Destory();
    usrAlloc->Destory();
    return err;
}

error_t ODEImplPIDThread::AllocatePendingWork()
{
    _workPending = DYN_LIST_CREATE(ODEWorkHandler *);
    
    if (!_workPending)
        return kErrorNotImplemented;

    return kStatusOkay;
}

void ODEImplPIDThread::PopCompletedTask(ODEWorkHandler * & current, bool & hasNextJob, ODEWorkHandler * & nextJob)
{
    error_t err;
    size_t length;
    ODEWorkHandler ** cur;

    // get first entry
    err = dyn_list_get_by_index(_workPending, 0, (void **)&cur);
    ASSERT(NO_ERROR(err), "Error: 0x%zx", err);
    current = *cur;

    // remove first entry
    err = dyn_list_remove(_workPending, 0);
    ASSERT(NO_ERROR(err), "Error: 0x%zx", err);

    // get array length
    err = dyn_list_entries(_workPending, &length);
    ASSERT(NO_ERROR(err), "Error: 0x%zx", err);

    // find next work item
    hasNextJob = length != 0;
    if (!hasNextJob)
    {
        nextJob = nullptr;
        return;
    }

    // get next job
    err = dyn_list_get_by_index(_workPending, 0, (void **)&cur);
    ASSERT(NO_ERROR(err), "Error: 0x%zx", err);
    nextJob = *cur;
}

void ODEImplPIDThread::PreemptExecution(pt_regs * registers, bool kick)
{
    linux_thread_info * info;

    info = GetInfoForTask(_task);
    info->next_user = *registers;
    info->swap_bool.doWork = 1;

    if (kick)
    {
        if (!ez_linux_caller(kallsyms_lookup_name("wake_up_state"), (size_t)_task, TASK_INTERRUPTIBLE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0))
        {
            kick_process(_task);
        }
    }

    LogPrint(kLogDbg, "APC: kicked/preempted usermode thread [%p / %i]", _task, ProcessesGetPid(_task));
}

void ODEImplPIDThread::PreemptExecutionForWork(ODEWorkHandler * exec, bool kick)
{
    pt_regs regs = { 0 };
    size_t rsp, krsp;

    rsp  = _stack.user.top;
    krsp = _stack.kernel.sp;

    rsp  -= sizeof(size_t);
    krsp -= sizeof(size_t);

    *(size_t*)krsp = (size_t)_proc->GetReturnAddress();

    regs.rsp = rsp;
    exec->ParseRegisters(regs);
    PreemptExecution(&regs, kick);
}

error_t GetDEThread(ODEImplPIDThread * & thread, task_k task)
{
    ODEImplProcess * proc;
    error_t err;

    err = GetDEProcess(proc, (task_k)task_get_group_leader_size_t(task));
    if (ERROR(err))
        return err;

    err = proc->GetThread(task, thread);
    if (ERROR(err))
        return err;

    return kStatusOkay;
}

error_t GetOrCreateDEThread(ODEImplPIDThread * & thread, task_k task)
{
    ODEImplProcess * proc;
    error_t err;

    err = GetDEProcess(proc, (task_k)task_get_group_leader_size_t(task));
    if (ERROR(err))
        return err;

    thread = proc->GetOrCreateThread(task);
    if (!thread)
        return kErrorOutOfMemory;

    return kStatusOkay;
}
