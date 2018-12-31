/*
    Purpose: oh boyyy fuck me
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson

    Issues:
      Undefined behaviour on thread crash; will leak and fuck up shit for all threads assigned to old bad pid (other tgid threads and derivatives will be ok)
      AHHHH
      AHHHH
      More AHHHH
*/
#include <libos.hpp>
#include "ODeferredExecution.hpp"
#include <Core/Memory/Linux/OLinuxMemory.hpp>
#include "../Processes/OProcesses.hpp"
#include "../../Utils/RCU.hpp"

struct APCStack
{
    page_k pages[6];
    size_t length;
    struct
    {
        union
        {
            size_t address;
            size_t bottom;
        };
        size_t top;
    } mapped;
};

struct linux_thread_info // TODO: portable structs. NEVER TRUST MSVC and GCC TO AGREE
{
    l_unsignedlong flags;
    u32      state;
    atomic_t swap_bool;
    pt_regs  previous_user;
    pt_regs  next_user;
};

static page_k  work_returnstub;
static chain_p work_queues;            // chain<tgid, chain<tid, ODEWorkHandler>>
static chain_p work_thread_stacks;     // chain<tgid, chain<tid, APCStack>>
static chain_p work_process_ips;       // chain<tgid, size_t>>
static chain_p work_restore;           // chain<tgid, chain<tid, pt_regs>
static mutex_k work_mutex;
static mutex_k work_watcher_mutex;

static OPtr<OLMemoryInterface> OS_MemoryInterface;

static void APC_AddPendingWork(task_k tsk, ODEWorkHandler * impl);
static void APC_GetTaskStack_s(task_k tsk, APCStack & stack);
static void APC_GetProcessReturnStub_s(task_k tsk, size_t & ret);
static void APC_AddPendingWork_s(task_k tsk, ODEWorkHandler * impl);
static void APC_PreemptThread_s(task_k task, pt_regs * registers, bool kick);

static linux_thread_info * GetInfoForTask(task_k tsk)
{
    return (linux_thread_info *)task_get_thread_info(tsk);
}

ODEWorkJobImpl::ODEWorkJobImpl(task_k task)
{
    _worker     = nullptr;
    _execd      = false;
    _dispatched = false;
    _work       = { 0 };
    _task       = task;
    ProcessesTaskIncrementCounter(task);
}

error_t ODEWorkJobImpl::SetWork(ODEWork & work)
{
    _work = work;
    return kStatusOkay;
}

error_t ODEWorkJobImpl::Schedule()
{
    if (_worker)
        return kErrorInternalError;

    _worker = new ODEWorkHandler(_task, this);
    if (!_worker)
    {
        mutex_unlock(work_watcher_mutex);
        return kErrorOutOfMemory;
    }
    _worker->SetWork(_work);
    _dispatched = true;

    return _worker->Schedule();
}

error_t ODEWorkJobImpl::HasDispatched(bool & dispatched) 
{
    dispatched = _dispatched;
    return kStatusOkay;
}

error_t ODEWorkJobImpl::HasExecuted(bool & executed)
{
    executed = _execd;
    return kStatusOkay;
}

error_t ODEWorkJobImpl::WaitExecute(uint32_t ms)               
{
    return kErrorNotImplemented;
}

error_t ODEWorkJobImpl::AwaitExecute(ODECompleteCallback_f cb) 
{
    return kErrorNotImplemented;
}

error_t ODEWorkJobImpl::GetResponse(size_t & ret)
{
    ret = _response;
    return kStatusOkay;
}

void ODEWorkJobImpl::InvalidateImp()
{
    mutex_lock(work_watcher_mutex);
    if (_worker)
        _worker->Fuckoff();
    mutex_unlock(work_watcher_mutex);

    ProcessesTaskDecrementCounter(_task);
}

void ODEWorkJobImpl::Fuckoff()
{
    _worker = nullptr;
}

void ODEWorkJobImpl::Hit(size_t response)
{
    _response = response;
    _execd    = true;
}

ODEWorkHandler::ODEWorkHandler(task_k tsk, ODEWorkJobImpl * worker)
{
     _tsk = tsk;
     _parant = worker;
     ProcessesTaskIncrementCounter(_tsk);
}

ODEWorkHandler::~ODEWorkHandler()
{
    if (_tsk)
        ProcessesTaskDecrementCounter(_tsk);
}

void ODEWorkHandler::ParseRegisters(pt_regs & regs)
{
    ORetardPtr<OLBufferDescription> desc;
    OPtr<OLGenericMappedBuffer> map;
    error_t err;
    APCStack stack;
    size_t rtstub;
    size_t rsp, krsp;
    size_t stackStart;

    APC_GetTaskStack_s(_tsk, stack);
    APC_GetProcessReturnStub_s(_tsk, rtstub);

    err = OS_MemoryInterface->NewBuilder(desc);
    ASSERT(err, "Couldn't allocate builder");

    for (int i = 0; i < 6; i++)
        desc->PageInsert(i, stack.pages[i]);

    err = desc->SetupKernelAddress(stackStart);
    ASSERT(err, "Couldn't setup kernel address for map");

    err = desc->MapKernel(map, OS_MemoryInterface->CreatePageEntry(OL_ACCESS_READ | OL_ACCESS_WRITE, kCacheNoCache));
    ASSERT(err, "Couldn't get map pages to kernel");

    err = map->GetVAEnd(krsp);
    ASSERT(err, "Couldn't get VA end");

    rsp  = stack.mapped.top;
    rsp  -= sizeof(size_t);
    krsp -= sizeof(size_t);
    *(size_t*)krsp = rtstub;

    regs.rsp = rsp;
    regs.rip = _work.address;
    regs.rdi = _work.parameters.one;
    regs.rsi = _work.parameters.two;
    regs.rdx = _work.parameters.three;
    regs.rcx = _work.parameters.four;
}

void ODEWorkHandler::Fuckoff()
{
    _parant = nullptr;
}

void ODEWorkHandler::Hit(size_t response)
{
    mutex_lock(work_watcher_mutex);
    if (_parant)
    {
        _parant->Hit(response);
        _parant->Fuckoff();
    }
    mutex_unlock(work_watcher_mutex);
    delete this;
}

error_t ODEWorkHandler::SetWork(ODEWork & work)
{
    _work = work;
    return kStatusOkay;
}

error_t ODEWorkHandler::Schedule()
{
    if (!_tsk)
        return kErrorInternalError;
    APC_AddPendingWork(_tsk, this);
    ProcessesTaskDecrementCounter(_tsk);
    _tsk = nullptr;
    return kFuckMe;
}

static void APC_MapReturnStub_s(task_k tsk, size_t & ret)
{
    error_t err;
    ORetardPtr<OLBufferDescription> desc;
    OPtr<OLGenericMappedBuffer> map;

    err = OS_MemoryInterface->NewBuilder(desc);
    ASSERT(NO_ERROR(err), "APC_AllocateStack_s: Couldn't allocate builder");

    desc->PageInsert(0, work_returnstub);

    err = desc->SetupUserAddress(tsk, ret);
    ASSERT(NO_ERROR(err), "APC_AllocateStack_s: couldn't setup address");

    err = desc->MapUser(map, OS_MemoryInterface->CreatePageEntry(OL_ACCESS_READ | OL_ACCESS_WRITE | OL_ACCESS_EXECUTE, kCacheNoCache));
    ASSERT(NO_ERROR(err), "APC_AllocateStack_s: Couldn't get map pages to kernel");
     
    map->DisableUnmapOnFree(); // release Xenus shit, allow task GC, keep VA mapping
}

static void APC_GetProcessReturnStub_s(task_k tsk, size_t & ret)
{
    error_t   err;
    uint_t    tgid;
    size_t    usraddr;
    size_t * pusraddr;

    tgid = ProcessesGetTgid(tsk);

    if (NO_ERROR(chain_get(work_process_ips, tgid, nullptr, (void **)&pusraddr)))
    {
        ret = *pusraddr;
        return;
    }

    // Map stub
    APC_MapReturnStub_s(tsk, usraddr);

    // Allocate link / map entry
    err = chain_allocate_link(work_process_ips, tgid, sizeof(size_t), nullptr, nullptr, (void **)&pusraddr);
    ASSERT(NO_ERROR(err), "APC_GetProcessReturnStub: couldn't create chain link");
    *pusraddr = usraddr;

    ret = usraddr;
}

static void APC_AllocateStack_s(task_k tsk, APCStack & stack)
{
    error_t err;
    ORetardPtr<OLBufferDescription> desc;
    OPtr<OLGenericMappedBuffer> map;

    for (int i = 0; i < 6; i++)
        stack.pages[i] = OS_MemoryInterface->AllocatePage(kPageNormal);

    stack.length = OS_THREAD_SIZE * 6;
 
    err = OS_MemoryInterface->NewBuilder(desc);
    ASSERT(err, "APC_AllocateStack_s: Couldn't allocate builder");

    for (int i = 0; i < 6; i++)
        desc->PageInsert(i, stack.pages[i]);

    err = desc->SetupUserAddress(tsk, stack.mapped.address);
    ASSERT(NO_ERROR(err), "APC_AllocateStack_s: couldn't setup address");

    err = desc->MapUser(map, OS_MemoryInterface->CreatePageEntry(OL_ACCESS_READ | OL_ACCESS_WRITE, kCacheNoCache));
    ASSERT(err, "APC_AllocateStack_s: Couldn't get map pages to kernel");

    err = map->GetVAEnd(stack.mapped.top);
    ASSERT(err, "APC_AllocateStack_s: Couldn't get VA end");

    err = map->GetVAStart(stack.mapped.bottom);
    ASSERT(err, "APC_AllocateStack_s: Couldn't get VA start");

    map->DisableUnmapOnFree(); // release Xenus shit, allow task GC, keep VA mapping
}

static void APC_GetTaskStack_s(task_k tsk, APCStack & outstack)
{
    error_t     err;
    uint_t      pid;
    uint_t      tgid;
    chain_p  * ppidchain;
    chain_p     pidchain;
    APCStack * pstack;
    APCStack    stack;

    tgid = ProcessesGetTgid(tsk);
    pid  = ProcessesGetPid(tsk);

    if (NO_ERROR(err = chain_get(work_thread_stacks, tgid, nullptr, (void **)&ppidchain)))
    {
        if (NO_ERROR(chain_get(*ppidchain, pid, nullptr, (void **)&pstack)))
        {
            // TGID exists, PID exists
            outstack = *pstack;
            return;
        }

        // No PID exists
        pidchain = *ppidchain;
    }
    else if (err == XENUS_ERROR_LINK_NOT_FOUND)
    {
        // No TGID and no subsequent chain of pids exists
        APCStack allocd;
        chain_p chain;

        // Allocate pid chain
        chain_allocate(&chain);
        
        // Link pid chain in root chain
        {
            chain_p * pchain;
            err = chain_allocate_link(work_thread_stacks, tgid, sizeof(chain_p), nullptr, nullptr, (void **)&pchain);
            ASSERT(NO_ERROR(err), "APC_GetTaskStack_s: couldn't create tgid link");
            *pchain = chain;
        }

        pidchain = chain;
    }
    else
    {
        panicf("APC_GetTaskStack_s: chain returned error code: 0x%zx", err);
    }

    // Allocate stack
    APC_AllocateStack_s(tsk, stack);

    // Allocate link / map entry
    {
        err = chain_allocate_link(pidchain, pid, sizeof(APCStack), nullptr, nullptr, (void **)&pstack);
        ASSERT(NO_ERROR(err), "APC_GetTaskStack_s: couldn't create chain link");
        *pstack = stack;
    }

    outstack = stack;
}

static void APC_Run_s(task_k tsk, ODEWorkHandler * impl, bool kick)
{
    pt_regs regs;
    impl->ParseRegisters(regs);
    APC_PreemptThread_s(tsk, &regs, kick);
}

static void APC_AddPendingWork_s(task_k tsk, ODEWorkHandler * impl)
{
    linux_thread_info * info;
    uint_t pid;
    uint_t tgid;
    error_t err;
    size_t length;
    ODEWorkHandler ** pimpl;
    dyn_list_head_p   listhead;
    chain_p * pchain;

    tgid = ProcessesGetTgid(tsk);
    pid  = ProcessesGetPid(tsk);

    // get or allocate array
    // map<task pid, dynamic array<ODEWorkhandler *>>
    if ((err = chain_get(work_queues, tgid, nullptr, (void **)&pchain)) == XENUS_ERROR_LINK_NOT_FOUND)
    {
        // No dynamic array
        dyn_list_head_p * plisthead;
        chain_p chain;

        chain_allocate(&chain);

        // Append thread group entry map into root 
        {
            err = chain_allocate_link(work_queues, tgid, sizeof(chain_p), nullptr, nullptr, (void **)&pchain);
            ASSERT(NO_ERROR(err), "APC_AddWorkWork_s: couldn't create chain link");
            *pchain = chain;
        }

        // Allocate list head/handle
        {
            listhead = DYN_LIST_CREATE(ODEWorkHandler *);
            ASSERT(listhead, "APC_AddWorkWork_s: list head couldn't be created - out of memory?");
        }

        // Allocate link / map entry
        {
            err = chain_allocate_link(chain, pid, sizeof(dyn_list_head_p), nullptr, nullptr, (void **)&plisthead);
            ASSERT(NO_ERROR(err), "APC_AddWorkWork_s: couldn't create chain link");
            *plisthead = listhead;
        }
    }
    else if (ERROR(err))
    {
        panicf("APC_AddWorkWork_s: bad chain. error code: 0x%zx", err);
    }
    else
    {
        // TGID root exists
        dyn_list_head_p * plisthead;
        chain_p pidmap = *pchain;

        // get map entry
        if ((err = chain_get(pidmap, pid, nullptr, (void **)&plisthead)) == XENUS_ERROR_LINK_NOT_FOUND)
        {
            // PID doesn't exist...

            // Allocate list head/handle
            listhead = DYN_LIST_CREATE(ODEWorkHandler *);
            ASSERT(listhead, "APC_AddWorkWork_s: list head couldn't be created - out of memory?");

            // Allocate link / map entry
            err = chain_allocate_link(pidmap, pid, sizeof(dyn_list_head_p), nullptr, nullptr, (void **)&plisthead);
            ASSERT(NO_ERROR(err), "APC_AddWorkWork_s: couldn't create chain link");
            *plisthead = listhead;
        }
        else if (ERROR(err))
        {
            panicf("APC_AddWorkWork_s: bad second chain. error code: 0x%zx", err);
        }
        else
        {
            // TGID exists, PID exists
            listhead = *plisthead;
        }
    }

    // append work entry to list 
    err = dyn_list_append(listhead, (void **)&pimpl);
    ASSERT(NO_ERROR(err), "couldn't append list entry");
    *pimpl = impl;

    // if there's only one entry in the work queue, kick start
    err = dyn_list_entries(listhead, &length);
    ASSERT(NO_ERROR(err), "couldn't get list size");

    if (length == 1)
        APC_Run_s(tsk, impl, true);
}

static void APC_PopComplete_s(task_k tsk, ODEWorkHandler * & job, bool & moreWorkPending, ODEWorkHandler * & next)
{
    link_p link;
    size_t length;
    dyn_list_head_p * listhead;
    chain_p * chainhead;
    ODEWorkHandler ** pimpl;

    // get dynamic array from map
    if (ERROR(chain_get(work_queues, ProcessesGetTgid(tsk), &link, (void **)&chainhead)))
    {
        LogPrint(kLogError, "1: APC_PopComplete_s failed... shit. TODO: Reece: how should we handle this?");
        return;
    }

    if (ERROR(chain_get(*chainhead, ProcessesGetPid(tsk), &link, (void **)&listhead)))
    {
        LogPrint(kLogError, "2: APC_PopComplete_s failed... shit. TODO: Reece: how should we handle this?");
        return;
    }

    // get first entry
    if (ERROR(dyn_list_get_by_index(*listhead, 0, (void **)&pimpl)))
    {
        LogPrint(kLogError, "3: APC_PopComplete_s failed... shit. TODO: Reece: how should we handle this?");
        return;
    }

    // ...as value. this pointer will be invalid after we...
    job = *pimpl;

    // ...nuke the first entry
    if (ERROR(dyn_list_remove(*listhead, 0)))
    {
        LogPrint(kLogError, "4: APC_PopComplete_s failed... shit. TODO: Reece: how should we handle this?");
        return;
    }

    // get array length
    if (ERROR(dyn_list_entries(*listhead, &length)))
    {
        LogPrint(kLogError, "5: APC_PopComplete_s failed... shit. TODO: Reece: how should we handle this?");
        return;
    }

    // find next work item
    moreWorkPending = length != 0;
    if (moreWorkPending)
    {
        if (ERROR(dyn_list_get_by_index(*listhead, 0, (void **)&next)))
        {
            LogPrint(kLogError, "6: APC_PopComplete_s failed... shit. TODO: Reece: how should we handle this?");
            return;
        }
    }
}

static void APC_CleanupTask_s(task_k tsk)
{
    link_p link;
    dyn_list_head_p * listhead;

    //if (NO_ERROR(chain_get(work_process_ips, ProcessesGetPid(tsk), &link, nullptr)))
    //    chain_deallocate_handle(link);
    //
    //if (NO_ERROR(chain_get(work_thread_stacks, ProcessesGetPid(tsk), &link, nullptr)))
    //    chain_deallocate_handle(link);
    //
    //if (NO_ERROR(chain_get(work_queues, ProcessesGetPid(tsk), &link, (void **)&listhead)))
    //{
    //    dyn_list_destory(*listhead);
    //    chain_deallocate_handle(link);
    //}
}

static void APC_PreemptThread_s(task_k task, pt_regs * registers, bool kick)
{
    linux_thread_info * info;

    info = GetInfoForTask(task);
    info->next_user          = *registers;
    info->swap_bool.counter  = 1;

    if (kick)
    {
        if (!ez_linux_caller(kallsyms_lookup_name("wake_up_state"), (size_t)task, TASK_INTERRUPTIBLE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0))
        {
            kick_process(task);
        }
    }
   
    LogPrint(kLogDbg, "APC: kicked/preempted usermode thread [%p / %i]", task, ProcessesGetPid(task));
}

static void APC_TryStoreSave(task_k task, pt_regs * restore)
{
    error_t    err;
    uint_t     pid;
    uint_t     tgid;
    chain_p  *pchain;
    chain_p    chain;
    pt_regs  *pregs;

    tgid = ProcessesGetTgid(task);
    pid  = ProcessesGetPid(task);

    // get or allocate array
    // map<task pid, dynamic array<ODEWorkhandler *>>
    if ((err = chain_get(work_restore, tgid, nullptr, (void **)&pchain)) == XENUS_ERROR_LINK_NOT_FOUND)
    {
        chain_p chain;
        chain_allocate(&chain);

        // Append thread group entry map into root 
        {
            err = chain_allocate_link(work_restore, tgid, sizeof(chain_p), nullptr, nullptr, (void **)&pchain);
            ASSERT(NO_ERROR(err), "APC_TryStoreSave: couldn't create chain link");
            *pchain = chain;
        }
    }
    else if (ERROR(err))
    {
        panicf("APC_TryStoreSave: [1] chain error %zx", err);
    }
    else
    {
        // get map entry
        if (NO_ERROR(err = chain_get(*pchain, pid, nullptr, (void **)&pregs)))
        {
            // PID exists
            return;
        } 

        ASSERT(err != XENUS_ERROR_LINK_NOT_FOUND, "APC_TryStoreSave: [2] chain error %zx", err)
    }

    chain = *pchain;

    err = chain_allocate_link(chain, pid, sizeof(pt_regs), nullptr, nullptr, (void **)&pregs);
    ASSERT(NO_ERROR(err), "couldn't allocate link. APC_TryStoreSave");
    *pregs = *restore;
}

static void APC_RestoreUserState_s(task_k task, pt_regs * fallback)
{  
    error_t     err;
    uint_t      pid;
    uint_t      tgid;
    chain_p  * pchain;
    pt_regs  * pregs;
    pt_regs     regs;

    tgid = ProcessesGetTgid(task);
    pid  = ProcessesGetPid(task);

    if (NO_ERROR(chain_get(work_restore, tgid, nullptr, (void **)&pchain)))
    {
        link_p link;
        if (NO_ERROR(chain_get(*pchain, pid, &link, (void **)&pregs)))
        {
            regs = *pregs;
            chain_deallocate_handle(link);

            LogPrint(kLogDbg, "APC: returning to usermode using stored registers - we had to deal with multiple items within the queue. RIP: %p", regs.rip);
            APC_PreemptThread_s(task, &regs, false);
            return;
        }
    }

    LogPrint(kLogDbg, "APC: returning to usermode without using stored registers - the queue should be emtpy. RIP: %p", fallback->rip);
    APC_PreemptThread_s(task, fallback, false);
}

static void APC_Complete_s(task_k task, size_t ret)
{
    bool moreWorkPending;
    ODEWorkHandler * next;
    ODEWorkHandler * cur;

    APC_PopComplete_s(task, cur, moreWorkPending, next);
    ASSERT(cur, "APC_PopComplete_s didn't pop an item from the FIFO work queue");

    cur->Hit(ret);

    if (moreWorkPending)
    {
        APC_TryStoreSave(task, &GetInfoForTask(task)->previous_user);
        APC_Run_s(task, next, false);
    }
    else
    {
        APC_RestoreUserState_s(task, &GetInfoForTask(task)->previous_user);
    }
}

static void APC_OnThreadExit_s(OPtr<OProcess> thread)
{
    task_k task;
    if (ERROR(thread->GetOSHandle((void **)&task)))
    {
        LogPrint(kLogError, "GetOSHandle failed within APC_OnThreadExit_s. Bad thread exit callback notification");
        return;
    }

    mutex_lock(work_mutex);
    APC_CleanupTask_s(task);
    mutex_unlock(work_mutex);
}

static void APC_OnThreadExit(OPtr<OProcess> thread)
{
    mutex_lock(work_mutex);
    APC_OnThreadExit_s(thread);
    mutex_unlock(work_mutex);
}

void DeferredExecFinish(size_t ret)
{
    mutex_lock(work_mutex);
    APC_Complete_s(OSThread, ret);
    mutex_unlock(work_mutex);
}

static void APC_AddPendingWork(task_k tsk, ODEWorkHandler * impl)
{
    mutex_lock(work_mutex);
    APC_AddPendingWork_s(tsk, impl);
    mutex_unlock(work_mutex);
}

static void InitReturnStub()
{
    const uint8_t x86_64[] = { 0x48, 0xC7, 0xC7, 0x05, 0x00, 0x00, 0x00, 0x48, 0x89, 0xC6, 0x48, 0xC7, 0xC0, 0x90, 0x01, 0x00, 0x00, 0x0F, 0x05 };

    size_t addr;
    error_t err;
    ORetardPtr<OLBufferDescription> desc;
    OPtr<OLGenericMappedBuffer> map;

    work_returnstub = OS_MemoryInterface->AllocatePage(kPageNormal);
    ASSERT(work_returnstub, "couldn't allocate return stub");

    err = OS_MemoryInterface->NewBuilder(desc);
    ASSERT(NO_ERROR(err), "Couldn't allocate builder");

    desc->PageInsert(0, work_returnstub);

    desc->SetupKernelAddress(addr);
    err = desc->MapKernel(map, OS_MemoryInterface->CreatePageEntry(OL_ACCESS_READ | OL_ACCESS_WRITE, kCacheNoCache));
    ASSERT(NO_ERROR(err), "Couldn't get map pages to kernel");

    err = map->GetVAStart(addr);
    ASSERT(NO_ERROR(err), "Couldn't get VA start");

    memcpy((void *)addr, x86_64, sizeof(x86_64));
}

void InitDeferredCalls()
{
    ProcessesAddExitHook(APC_OnThreadExit);

    ASSERT(NO_ERROR(GetLinuxMemoryInterface(OS_MemoryInterface)), "couldn't get linux memory interface"); // TODO: assert

    InitReturnStub();

    chain_allocate(&work_queues);
    chain_allocate(&work_thread_stacks);
    chain_allocate(&work_process_ips);
    chain_allocate(&work_restore);

    work_mutex         = mutex_init();
    work_watcher_mutex = mutex_init();
}

LIBLINUX_SYM error_t CreateWorkItem(OPtr<OProcessThread> target, const OOutlivableRef<ODEWorkJob> out)
{
    error_t idgaf;
    task_k handle;

    if (target->IsDead())
        return kErrorIllegalBadArgument;

    if (ERROR(idgaf = target->GetOSHandle((void **)&handle)))
        return idgaf;

    if (!out.PassOwnership(new ODEWorkJobImpl(handle)))
        return kErrorOutOfMemory;

    return kStatusOkay;
}