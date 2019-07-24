/*
    Purpose: A Windows-APC style thread preemption within the Linux kernel [depends on latest xenus linux kernel patch]
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#include <libos.hpp>
#include "ODECriticalSection.hpp"
#include "ODEThread.hpp"
#include "ODEProcess.hpp"
#include "ODEWork.hpp"
#include "ODeferredExecution.hpp"
#include "CallingConventions/CCManager.hpp"
#include <Core/CPU/OMemoryCoherency.hpp>
#include <Core/Utilities/OThreadUtilities.hpp>
#include <Core/Processes/OProcessTracking.hpp>
// Internal apis:
#include <Source/Core/Memory/Linux/OLinuxMemory.hpp>
#include <Source/Core/Processes/OProcesses.hpp>
#include <Source/Core/Processes/OProcessHelpers.hpp>
#include <Source/Core/Memory/Linux/x86_64/OLinuxMemoryPages.hpp>

struct linux_thread_info // TODO: portable structs. NEVER TRUST MSVC and GCC TO AGREE
{
    l_unsignedlong flags;
    u32      state;
    atomic_t swap_bool;
    pt_regs  previous_user;
    pt_regs  next_user;
};

static void APC_OnThreadExit(OPtr<OProcess> thread);

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
    DestroyState();
}

error_t ODEImplPIDThread::Init(task_k task)
{
    error_t err;

    _task = task;

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
    if (_task == task)
        return;
 
    DestroyState();
    Init(task);
}

void ODEImplPIDThread::DestroyState()
{
    DestroyStack();
    DestroyQueue();
    _userState.hasPreviousTask = false;
}

void ODEImplPIDThread::DestroyQueue()
{
    error_t err;

    if (!_workPending)
        return;

    err = dyn_list_iterate(_workPending, [](void * buffer, void * context)
    {
        ODEWorkHandler * work = *reinterpret_cast<ODEWorkHandler **>(buffer);
        if (work)
            work->Die();
    }, nullptr);
    ASSERT(NO_ERROR(err), "Error: " PRINTF_ERROR, err);

    err = dyn_list_destroy(_workPending);
    ASSERT(NO_ERROR(err), "Error: " PRINTF_ERROR, err);
}

void ODEImplPIDThread::DestroyStack()
{
    if (!_stack.pages)
        return;

    FreeLinuxPages(_stack.pages);
    _stack.kernel.allocation->Destroy();
    _stack.user.allocation->Destroy();
}

error_t ODEImplPIDThread::AppendWork(ODEWorkHandler * handler)
{
    error_t err;
    size_t count;
    ODEWorkHandler ** entry;

    err = dyn_list_append(_workPending, reinterpret_cast<void **>(&entry));
    if (ERROR(err))
        return err;

    *entry = handler;

    err = dyn_list_entries(_workPending, &count);
    ASSERT(NO_ERROR(err), "Error: " PRINTF_ERROR, err);

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
    Memory::OLVirtualAddressSpace * krnVas;
    ODumbPointer<Memory::OLVirtualAddressSpace> usrVas;
    OPtr<Memory::OLMemoryAllocation> usrAlloc;
    OPtr<Memory::OLMemoryAllocation> krnAlloc;

    err = g_memory_interface->GetKernelAddressSpace(OUncontrollableRef<Memory::OLVirtualAddressSpace>(krnVas));
    if (ERROR(err))
    {
        LogPrint(kLogError, "Couldn't obtain kernel address space interface, error " PRINTF_ERROR, err);
        return err;
    }

    err = g_memory_interface->GetUserAddressSpace(_task, OOutlivableRef<Memory::OLVirtualAddressSpace>(usrVas));
    if (ERROR(err))
    {
        LogPrint(kLogError, "Couldn't obtain user address space interface, error " PRINTF_ERROR, err);
        return err;
    }

    err = krnVas->NewDescriptor(0, APC_STACK_PAGES, OOutlivableRef<Memory::OLMemoryAllocation>(krnAlloc));
    if (ERROR(err))
    {
        LogPrint(kLogError, "Couldn't allocate descriptor, error " PRINTF_ERROR, err);
        return err;
    }

    err = usrVas->NewDescriptor(0, APC_STACK_PAGES, OOutlivableRef<Memory::OLMemoryAllocation>(usrAlloc));
    if (ERROR(err))
    {
        LogPrint(kLogError, "Couldn't allocate descriptor, error " PRINTF_ERROR, err);
        krnAlloc->Destroy();
        return err;
    }

    _stack.pages = usrVas->AllocatePFNs(Memory::kPageNormal, APC_STACK_PAGES, false, Memory::OL_PAGE_ZERO);
    if (!_stack.pages)
    {
        krnAlloc->Destroy();
        usrAlloc->Destroy();
        return kErrorOutOfMemory;
    }

    for (size_t i = 0; i < APC_STACK_PAGES; i++)
    {
        Memory::OLPageEntry entry;

        entry.meta = g_memory_interface->CreatePageEntry(Memory::OL_ACCESS_READ | Memory::OL_ACCESS_WRITE, Memory::kCacheNoCache);
        entry.type = Memory::kPageEntryByPFN;
        entry.pfn = _stack.pages[i].pfn;

        err = krnAlloc->PageInsert(i, entry);
        if (ERROR(err))
        {
            LogPrint(kLogError, "Couldn't insert page into kernel address space " PRINTF_ERROR, err);
            goto error;
        }

        err = usrAlloc->PageInsert(i, entry);
        if (ERROR(err))
        {
            LogPrint(kLogError, "Couldn't insert page into user address space " PRINTF_ERROR, err);
            goto error;
        }
    }
    
    _stack.user.allocation   = usrAlloc;
    _stack.user.allocStart   = usrAlloc->GetStart();
    _stack.user.allocEnd     = usrAlloc->GetEnd();

    _stack.kernel.allocation = krnAlloc;
    _stack.kernel.allocStart = krnAlloc->GetStart();
    _stack.kernel.allocEnd   = krnAlloc->GetEnd();
    return kStatusOkay;

error:
    usrVas->FreePages(_stack.pages);
    krnAlloc->Destroy();
    usrAlloc->Destroy();
    return err;
}

error_t ODEImplPIDThread::AllocatePendingWork()
{
    _workPending = DYN_LIST_CREATE(ODEWorkHandler *);
    
    if (!_workPending)
        return kErrorOutOfMemory;

    return kStatusOkay;
}

void ODEImplPIDThread::PopCompletedTask(ODEWorkHandler * & current, bool & hasNextJob, ODEWorkHandler * & nextJob)
{
    error_t err;
    size_t length;
    ODEWorkHandler ** cur;

    // get first entry
    err = dyn_list_get_by_index(_workPending, 0, reinterpret_cast<void **>(&cur));
    ASSERT(NO_ERROR(err), "Error: " PRINTF_ERROR, err);
    current = *cur;

    // remove first entry
    err = dyn_list_remove(_workPending, 0);
    ASSERT(NO_ERROR(err), "Error: " PRINTF_ERROR, err);

    // get array length
    err = dyn_list_entries(_workPending, &length);
    ASSERT(NO_ERROR(err), "Error: " PRINTF_ERROR, err);

    // find next work item
    hasNextJob = length != 0;
    if (!hasNextJob)
    {
        nextJob = nullptr;
        return;
    }

    // get next job
    err = dyn_list_get_by_index(_workPending, 0, reinterpret_cast<void **>(&cur));
    ASSERT(NO_ERROR(err), "Error: " PRINTF_ERROR, err);
    nextJob = *cur;
}

void ODEImplPIDThread::PreemptExecution(pt_regs * registers, bool kick)
{
    linux_thread_info * info;

    info = GetInfoForTask(_task);
    info->next_user = *registers;
    info->swap_bool.counter = 1;

    CPU::Memory::ReadWriteBarrier();

    if (kick)
    {
        int state = task_get_state_int32(_task);
        
        if (state == 1)
            wake_up_process(_task);

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
    size_t ursp, krsp, kdif = 0;
    size_t * hacked;
    ICallingConvention * convention;
    const ODEWork & work = exec->GetWork();
    bool bits = Utilities::Tasks::IsTask32Bit(_task);

    convention = ODEGetConvention(work.cc);
    ASSERT(convention, "couldn't locate calling convention");

    ursp = _stack.user.stackTop;
    krsp = _stack.kernel.stackTop;

    ursp -= bits ? sizeof(uint32_t) : sizeof(uint64_t);
    krsp -= bits ? sizeof(uint32_t) : sizeof(uint64_t);

    if (bits)
        *(uint32_t*)krsp = _proc->GetReturnAddress();
    else
        *(uint64_t*)krsp = _proc->GetReturnAddress();

    regs.rsp = ursp;
    
    convention->SetupRegisters(work, regs);
    hacked = convention->SetupStack(work, reinterpret_cast<size_t *>(krsp));

    ursp -= krsp - reinterpret_cast<size_t>(hacked);

    PreemptExecution(&regs, kick);
}

error_t GetDEThread(ODEImplPIDThread * & thread, task_k task)
{
    ODEImplProcess * proc;
    error_t err;

    err = GetDEProcess(proc, ProcessesGetProcess(task));
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

    err = GetDEProcess(proc, ProcessesGetProcess(task));
    if (ERROR(err))
        return err;

    thread = proc->GetOrCreateThread(task);
    if (!thread)
        return kErrorOutOfMemory;

    return kStatusOkay;
}

void InitDEThreads()
{
    ProcessesAddExitHook(APC_OnThreadExit);
}

static void APC_OnThreadExit(OPtr<OProcess> thread)
{
    void * handle;
    error_t err;

    EnterDECriticalSection();

    err = thread->GetOSHandle(&handle);
    ASSERT(NO_ERROR(err), "Error: " PRINTF_ERROR, err);

    FreeDEProcess((task_k)handle);

    LeaveDECriticalSection();
}

void APC_OnThreadExecFinish(size_t ret)
{
    error_t err;
    ODEImplPIDThread * thread;

    EnterDECriticalSection();

    err = GetDEThread(thread, OSThread);
    if (ERROR(err))
    {
        LogPrint(kLogError, "Couldn't acquire DE thread instance... SYSCALL ABUSE? (error: " PRINTF_ERROR ")", err);
        LeaveDECriticalSection();
        return;
    }

    thread->NtfyJobFinished(ret);

    LeaveDECriticalSection();
}

error_t APC_AddPendingWork(task_k tsk, ODEWorkHandler * impl)
{
    error_t err;
    ODEImplPIDThread * thread;

    EnterDECriticalSection();

    thread = nullptr;
    err = GetOrCreateDEThread(thread, tsk);
    if (ERROR(err))
    {
        LogPrint(kLogError, "Couldn't create DE thread instance (error: " PRINTF_ERROR ")", err);
        LeaveDECriticalSection();
        return err;
    }

    thread->AppendWork(impl);

    LeaveDECriticalSection();
    return kStatusOkay;
}
