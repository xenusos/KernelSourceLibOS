/*
    Purpose: A Windows-APC style thread preemption within the Linux kernel [depends on latest xenus linux kernel patch]
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
    Issues:
      Partial leak on APC creation (thread restore queue) if the threaed has yet to be used. cleaned up on thread exit. not really a leak, but still something to consider fixing.
      None known yet.

*/
#include <libos.hpp>
#include <Core/CPU/OWorkQueue.hpp>
#include "ODeferredExecution.hpp"

#include "../Memory/Linux/OLinuxMemory.hpp"
#include "../Processes/OProcesses.hpp"
#include "../../Utils/RCU.hpp"

struct linux_thread_info // TODO: portable structs. NEVER TRUST MSVC and GCC TO AGREE
{
    l_unsignedlong flags;
    u32      state;
    atomic_t swap_bool;
    pt_regs  previous_user;
    pt_regs  next_user;
};

struct RegRestoreQueue
{
    bool hasPreviousTask;
    pt_regs restore;
};

static page_k  work_returnstub_64;
static chain_p work_queues;            // chain<tgid, chain<tid, ODEWorkHandler>>
static chain_p work_thread_stacks;     // chain<tgid, chain<tid, APCStack>>
static chain_p work_process_ips;       // chain<tgid, size_t>>
static chain_p work_restore;           // chain<tgid, chain<tid, pt_regs>
static mutex_k work_mutex;
static mutex_k work_watcher_mutex;

static error_t APC_AddPendingWork(task_k tsk, ODEWorkHandler * impl);
static error_t APC_GetTaskStack_s(task_k tsk, APCStack & stack);
static error_t APC_GetProcessReturnStub_s(task_k tsk, size_t & ret);
static error_t APC_AddPendingWork_s(task_k tsk, ODEWorkHandler * impl);
static void APC_PreemptThread_s(task_k task, pt_regs * registers, bool kick);

static linux_thread_info * GetInfoForTask(task_k tsk)
{
    return (linux_thread_info *)task_get_thread_info(tsk);
}

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////// Work object watcher / API ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

ODEWorkJobImpl::ODEWorkJobImpl(task_k task, OPtr<OWorkQueue> workqueue)
{
    _worker     = nullptr;
    _execd      = false;
    _dispatched = false;
    _work       = { 0 };
    _task       = task;
    _cb         = nullptr;
    _workqueue = workqueue;

    ProcessesTaskIncrementCounter(task);
}

error_t ODEWorkJobImpl::SetWork(ODEWork & work)
{
    CHK_DEAD;
    _work = work;
    return kStatusOkay;
}

error_t ODEWorkJobImpl::Schedule()
{
    CHK_DEAD;
    error_t err;

    if (_worker)
        return kErrorInternalError;

    _worker = new ODEWorkHandler(_task, this);

    if (!_worker)
        return kErrorOutOfMemory;

    if (ERROR(err = _worker->Construct()))
    {
        delete _worker;
        return err;
    }

    _worker->SetWork(_work);
    _dispatched = true;

    return _worker->Schedule();
}

error_t ODEWorkJobImpl::HasDispatched(bool & dispatched) 
{
    CHK_DEAD;
    dispatched = _dispatched;
    return kStatusOkay;
}

error_t ODEWorkJobImpl::HasExecuted(bool & executed)
{
    CHK_DEAD;
    executed = _execd;
    return kStatusOkay;
}

error_t ODEWorkJobImpl::WaitExecute(uint32_t ms)               
{
    CHK_DEAD;
    error_t err;
    if (STRICTLY_OKAY(err = _workqueue->WaitAndAddOwner(ms)))
        _workqueue->ReleaseOwner();
    return err;
}

error_t ODEWorkJobImpl::AwaitExecute(ODECompleteCallback_f cb, void * context)
{
    CHK_DEAD;
    if (_cb)
        return kErrorInternalError;
    _cb     = cb;
    _cb_ctx = context;
    return kStatusOkay;
}

error_t ODEWorkJobImpl::GetResponse(size_t & ret)
{
    CHK_DEAD;
    ret = _response;
    return kStatusOkay;
}

void ODEWorkJobImpl::InvalidateImp()
{
    mutex_lock(work_watcher_mutex);
    if (_worker)
        _worker->Fuckoff();
    mutex_unlock(work_watcher_mutex);

    _workqueue->Destory();

    if (_task)
        ProcessesTaskDecrementCounter(_task);
}

void ODEWorkJobImpl::Fuckoff()
{
    _worker = nullptr;
}

void ODEWorkJobImpl::Trigger(size_t response)
{
    _response = response;
    _execd    = true;
}

void ODEWorkJobImpl::GetCallback(ODECompleteCallback_f & callback, void * & context)
{
    callback = _cb;
    context  = _cb_ctx;
}

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////// Work Handler [APC object] ///////////////////////////////////
//  This doesn't follow the regular conventions in this codebase                          //
//   nor does this follow common practices                                                //
////////////////////////////////////////////////////////////////////////////////////////////

ODEWorkHandler::ODEWorkHandler(task_k tsk, ODEWorkJobImpl * worker)
{
     _rtstub          = 0;
     _tsk             = tsk;
     _parant          = worker;
     ProcessesTaskIncrementCounter(_tsk);
}

ODEWorkHandler::~ODEWorkHandler()
{
    if (_tsk)
        ProcessesTaskDecrementCounter(_tsk);

    if (_kernel_map.allocation.GetTypedObject())
        _kernel_map.allocation->Destory();
}

error_t ODEWorkHandler::AllocateStack()
{
    error_t ret;
    mutex_lock(work_mutex);
    ret = APC_GetTaskStack_s(_tsk, _stack);
    mutex_unlock(work_mutex);
    return ret;
}

error_t ODEWorkHandler::AllocateStub()
{
    error_t ret;
    mutex_lock(work_mutex);
    ret = APC_GetProcessReturnStub_s(_tsk, _rtstub);
    mutex_unlock(work_mutex);
    return ret;
}

error_t ODEWorkHandler::MapToKernel()
{
    error_t err;
    OLVirtualAddressSpace * vas;

    err = g_memory_interface->GetKernelAddressSpace(OUncontrollableRef<OLVirtualAddressSpace>(vas));
    if (ERROR(err))
    {
        LogPrint(kLogError, "Couldn't allocate builder, error 0x%zx", err);
        return err;
    }

    err = vas->NewDescriptor(0, APC_STACK_PAGES, OOutlivableRef<OLMemoryAllocation>(_kernel_map.allocation));
    if (ERROR(err))
    {
        LogPrint(kLogError, "Couldn't allocate descriptor, error 0x%zx", err);
        return err;
    }

    for (size_t i = 0; i < APC_STACK_PAGES; i++)
    {
        error_t err;
        OLPageEntry entry;

        entry.meta = g_memory_interface->CreatePageEntry(OL_ACCESS_READ | OL_ACCESS_WRITE, kCacheNoCache);
        entry.type = kPageEntryByPage;
        entry.page = _stack.pages[i];

        err = _kernel_map.allocation->PageInsert(i, entry);
        if (ERROR(err))
        {
            LogPrint(kLogError, "couoldn't insert page into kerel 0x%zx", err);
            return err;
        }
    }

    _kernel_map.address = _kernel_map.allocation->GetStart();
    _kernel_map.sp      = _kernel_map.allocation->GetEnd();
    return kStatusOkay;
}

error_t ODEWorkHandler::Construct()
{
    error_t err;

    if (ERROR(err = AllocateStack()))
        return err;

    if (ERROR(err = AllocateStub()))
        return err;

    if (ERROR(err = MapToKernel()))
        return err;

    return kStatusOkay;
}

void ODEWorkHandler::ParseRegisters(pt_regs & regs)
{
    error_t err;
    size_t rsp, krsp;
    size_t stackStart;

    rsp  = _stack.mapped.top;
    krsp = _kernel_map.sp;

    rsp  -= sizeof(size_t);
    krsp -= sizeof(size_t);

    *(size_t*)krsp = _rtstub;
    // copy is effective in usermode 

    regs.rsp = rsp;
    regs.rip = _work.address;


    if (_work.cc == kODESysV)
    {
        regs.rdi = _work.parameters.one;
        regs.rsi = _work.parameters.two;
        regs.rdx = _work.parameters.three;
        regs.rcx = _work.parameters.four;
    }
    else if (_work.cc == kODEWin64)
    {
        regs.rcx = _work.parameters.one;
        regs.rdx = _work.parameters.two;
        regs.r8  = _work.parameters.three;
        regs.r9  = _work.parameters.four;
    }
}

void ODEWorkHandler::Fuckoff()
{
    _parant = nullptr;
}

void ODEWorkHandler::Hit(size_t response)
{
    ODECompleteCallback_f callback = nullptr;
    void * context                 = nullptr;
    
    mutex_lock(work_watcher_mutex);
    if (_parant)
    {
        _parant->Trigger(response);
        _parant->GetCallback(callback, context);
        _parant->Fuckoff();
    }
    mutex_unlock(work_watcher_mutex);
    
    if (callback)
        callback(context);
    
    delete this;
}

void ODEWorkHandler::Die()
{
    mutex_lock(work_watcher_mutex);
    if (_parant)
    {
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
    error_t err;

    if (!_tsk)
        return kErrorInternalError;

    err = APC_AddPendingWork(_tsk, this);

    if (ERROR(err))
        return err;

    ProcessesTaskDecrementCounter(_tsk);
    _tsk = nullptr;
    return kStatusOkay;
}

////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////// APC implementation  //////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

static error_t APC_MapReturnStub_s(task_k tsk, size_t & ret)
{
    error_t err;
    OLPageEntry entry;
    ODumbPointer<OLMemoryAllocation> desc;
    ODumbPointer<OLVirtualAddressSpace> vas;

    err = g_memory_interface->GetUserAddressSpace(tsk, OOutlivableRef<OLVirtualAddressSpace>(vas));
    if (ERROR(err))
        return err;

    err = vas->NewDescriptor(0, 1, OOutlivableRef<OLMemoryAllocation>(desc));
    if (ERROR(err))
        return err;

    entry.meta = g_memory_interface->CreatePageEntry(OL_ACCESS_READ | OL_ACCESS_WRITE | OL_ACCESS_EXECUTE, kCacheNoCache);
    entry.type = kPageEntryByPage;
    entry.page = work_returnstub_64;

    err = desc->PageInsert(0, entry);
    if (ERROR(err))
    {
        LogPrint(kLogError, "Couldn't insert page into builder, 0x%zx", err);
        return err;
    }
    
    ret = desc->GetStart();

    desc->ForceLinger(); // do not free, when we return to the caller
    return kStatusOkay;
}

static bool APC_GetProcessReturnStubCache_s(task_k tsk, error_t & error, size_t & ret)
{
    error_t   err;
    uint_t    tgid;
    size_t    usraddr;
    size_t * pusraddr;

    tgid = ProcessesGetTgid(tsk);

    if (NO_ERROR(error = chain_get(work_process_ips, tgid, nullptr, (void **)&pusraddr)))
    {
        ret = *pusraddr;
        return true;
    }
    else if (error == kErrorLinkNotFound)
    {
        error = kStatusOkay;
        return false;
    }
    else
    {
        return false;
    }
}

static error_t APC_AllocateAndLinkProcessReturnStub_s(task_k tsk, size_t & ret)
{
    error_t err;
    size_t * pusraddr;
    uint_t    tgid;

    tgid = ProcessesGetTgid(tsk);

    if (ERROR(err = APC_MapReturnStub_s(tsk, ret)))
        return err;

    err = chain_allocate_link(work_process_ips, tgid, sizeof(size_t), nullptr, nullptr, (void **)&pusraddr);
   
    if (ERROR(err))
        return err; // no clean up is required; we do that on process exit

    *pusraddr = ret;

    return kStatusOkay;
}

static error_t APC_GetProcessReturnStub_s(task_k tsk, size_t & ret)
{
    error_t err;
    chain_p chain;
    bool    found;

    found = APC_GetProcessReturnStubCache_s(tsk, err, ret);

    if (found)
    {
        ASSERT(NO_ERROR(err), "found APC return handler stub, but an error occurred.");
        return kStatusOkay;
    }

    if (ERROR(err))
        return err;

    return APC_AllocateAndLinkProcessReturnStub_s(tsk, ret);
}

static void APC_ReleaseStack_s(APCStack * stack)
{
    error_t err;
    ODumbPointer<OLVirtualAddressSpace> vas;

    err = g_memory_interface->GetUserAddressSpace(stack->tsk, OOutlivableRef<OLVirtualAddressSpace>(vas));
    ASSERT(NO_ERROR(err), "GetUserAddressSpace failed: 0x%zx", err);

    vas->FreePages(stack->pages);
}

static error_t APC_AllocateStack_s(task_k tsk, APCStack & stack)
{
    error_t err;
    page_k * pages;
    ODumbPointer<OLMemoryAllocation> desc;
    ODumbPointer<OLVirtualAddressSpace> vas;

    err = g_memory_interface->GetUserAddressSpace(tsk, OOutlivableRef<OLVirtualAddressSpace>(vas));
    if (ERROR(err))
        return err;

    err = vas->NewDescriptor(0, APC_STACK_PAGES, OOutlivableRef<OLMemoryAllocation>(desc));
    if (ERROR(err))
        return err;

    pages = vas->AllocatePages(kPageNormal, 6, false, OL_PAGE_ZERO);
    if (!pages)
        return kErrorOutOfMemory;

    stack.tsk    = tsk;
    stack.pages  = pages;
    stack.length = APC_STACK_PAGES << OS_PAGE_SHIFT;

    for (size_t i = 0; i < APC_STACK_PAGES; i++)
    {
        OLPageEntry entry;

        entry.meta = g_memory_interface->CreatePageEntry(OL_ACCESS_READ | OL_ACCESS_WRITE, kCacheNoCache);
        entry.type = kPageEntryByPage;
        entry.page = pages[i];

        err = desc->PageInsert(i, entry);

        if (ERROR(err))
        {
            LogPrint(kLogError, "Couldn't insert page into builder, 0x%zx", err);
            goto errorFreePages;
        }
    }

    stack.mapped.top    = desc->GetEnd();
    stack.mapped.bottom = desc->GetStart();

    desc->ForceLinger(); // do not free, when we return to the caller
    return kStatusOkay;

errorFreePages:

    APC_ReleaseStack_s(&stack);
    return err;
}

/**
   Given "static chain_p work_thread_stacks;     // chain<tgid, chain<tid, APCStack>>",
    returns outstack: the stack of the thread (if such exists) 
    returns pidchain: the second pid chain value within the root chain
    returns <bool>  : has APC stack been found
*/
static bool APC_GetTaskStackCache_s(task_k tsk, error_t & error, chain_p & pidchain, APCStack & outstack)
{
    error_t     err;
    uint_t      pid;
    uint_t      tgid;
    chain_p  * ppidchain;
    APCStack * pstack;

    tgid = ProcessesGetTgid(tsk);
    pid  = ProcessesGetPid(tsk);

    pidchain = nullptr;
    error    = kStatusOkay;

    if (NO_ERROR(err = chain_get(work_thread_stacks, tgid, nullptr, (void **)&ppidchain)))
    {
        pidchain = *ppidchain;

        if (NO_ERROR(err == chain_get(pidchain, pid, nullptr, (void **)&pstack)))
        {
            // TGID exists, PID exists
            error = kStatusOkay;
            outstack = *pstack;
            return true;
        }
        else if (err == kErrorLinkNotFound)
        {
            // No PID / APC exists
            error = kStatusOkay;
            return false;
        }
        else 
        {
            // something bad happened :/
            error = err;
            return false;
        }
    
    }
    else if (err == kErrorLinkNotFound)
    {
        chain_p   chain;
        chain_p * pchain;

        // Allocate pid chain
        err = chain_allocate(&chain);

        if (ERROR(err))
            goto errorCondition;

        // Link pid chain in root chain
        err = chain_allocate_link(work_thread_stacks, tgid, sizeof(chain_p), nullptr, nullptr, (void **)&pchain);

        if (ERROR(err))
            goto errorCondition;

        *pchain = chain;

        pidchain = chain;
        outstack = { 0 };
        error = kStatusOkay;
        return false;
    }
    else
    {
        // something bad happened :/
        outstack = { 0 };
        error = err;
        return false;
    }

errorCondition:
    {
        error = err;
        return false;
    }
}

/***
    Given a task struct and chain, allocates an APC stack, and append it to the pidchain
*/
static error_t APC_AllocateAndLinkStack_s(task_k task, chain_p pidchain, APCStack & outstack)
{
    error_t     err;
    uint_t      pid;
    APCStack * pstack;

    pid = ProcessesGetPid(task);

    if (ERROR(err = APC_AllocateStack_s(task, outstack)))
        return err;

    err = chain_allocate_link(pidchain, pid, sizeof(APCStack), nullptr, nullptr, (void **)&pstack);

    if (ERROR(err))
    {
        APC_ReleaseStack_s(&outstack);
        return err;
    }

    *pstack = outstack;
    return kStatusOkay;
}

static error_t APC_GetTaskStack_s(task_k tsk, APCStack & outstack)
{
    error_t err;
    chain_p chain;
    bool    found;

    found = APC_GetTaskStackCache_s(tsk, err, chain, outstack);
    
    if (found)
    {
        ASSERT(NO_ERROR(err), "found APC stack, but an error occurred.");
        return kStatusOkay;
    }

    if (ERROR(err))
        return err;

    return APC_AllocateAndLinkStack_s(tsk, chain, outstack);
}

static void APC_Run_s(task_k tsk, ODEWorkHandler * impl, bool kick)
{
    pt_regs regs;
    impl->ParseRegisters(regs);
    APC_PreemptThread_s(tsk, &regs, kick);
}

static error_t APC_WQ_RegisterTGID_s(uint_t tgid, chain_p & chain, link_p & link)
{
    error_t err;
    chain_p * pchain;

    if (ERROR(err = chain_allocate(&chain)))
        return err;

    err = chain_allocate_link(work_queues, tgid, sizeof(chain_p), nullptr, &link, (void **)&pchain);

    if (ERROR(err))
        return err;

    *pchain = chain;
    return kStatusOkay;
}

static error_t APC_WQ_RegisterPID_s(chain_p chain, uint_t pid, dyn_list_head_p & list, link_p & handle)
{
    error_t err;
    dyn_list_head_p * plist;

    list = DYN_LIST_CREATE(ODEWorkHandler *);
    if (!list)
        return kErrorOutOfMemory;

    err = chain_allocate_link(chain, pid, sizeof(dyn_list_head_p), nullptr, &handle, (void **)&plist);

    if (ERROR(err))
    {
        ASSERT(NO_ERROR(dyn_list_destory(list)), "invalid list");
        return err;
    }

    *plist = list;
    return kStatusOkay;
}

static error_t APC_WQ_TrySchedule_s(task_k tsk, ODEWorkHandler * impl, dyn_list_head_p listhead)
{
    error_t err;
    size_t length;

    err = dyn_list_entries(listhead, &length);

    if (ERROR(err))
        return err;

    if (length == 1)
        APC_Run_s(tsk, impl, OSThread != tsk);

    return kStatusOkay;
}

static error_t APC_WQ_PreallocateRegQueue(uint_t tgid, uint_t pid)
{
    error_t           err;
    chain_p   *       pchain;
    chain_p           chain;
    link_p            lhandle;
    RegRestoreQueue  *pqueue;
    RegRestoreQueue   queue;

    lhandle = nullptr;

    if (NO_ERROR(err = chain_get(work_restore, tgid, nullptr, (void **)&pchain)))
    {
        // get map entry
        if (NO_ERROR(err = chain_get(*pchain, pid, &lhandle, (void **)&pqueue)))
        {
            // PID exists
            return kStatusOkay;
        }
        else if (err == kErrorLinkNotFound)
        {
            goto allocatePidQueue;
        }
        else
        {
            return err;
        }
    }
    else if (err == kErrorLinkNotFound)
    {
        chain_p chain;

        err = chain_allocate(&chain);
        if (ERROR(err))
            return err;

        err = chain_allocate_link(work_restore, tgid, sizeof(chain_p), nullptr, &lhandle, (void **)&pchain);
        if (ERROR(err))
        {
            ASSERT(NO_ERROR(chain_destory(chain)), "couldn't destory chain; leaking memory");
            return err;
        }

        *pchain = chain;

        goto allocatePidQueue;
    }
    else
    {
        return err;
    }

allocatePidQueue:
    chain = *pchain;

    err = chain_allocate_link(chain, pid, sizeof(RegRestoreQueue), nullptr, nullptr, (void **)&pqueue);
    if (ERROR(err))
    {
        if (lhandle)
        {
            ASSERT(NO_ERROR(chain_deallocate_handle(lhandle)), "couldn't destory pid handle; leaking memory");
        }
        return err;
    }

    queue.restore         = { 0 };
    queue.hasPreviousTask = false;

    *pqueue = queue;

    return kStatusOkay;
}

static error_t APC_AddPendingWork_s(task_k tsk, ODEWorkHandler * impl)
{
    uint_t pid;
    uint_t tgid;
    error_t err;
    dyn_list_head_p   listhead;
    chain_p * pchain;
    link_p pidHandle;

    // Obtain prerequisite task data
    {
        tgid = ProcessesGetTgid(tsk);
        pid = ProcessesGetPid(tsk);
    }

    // Preallocate register storage for the thread. if we decide to jump to another work item rather than the callee, we need a place to store the original interrupted registers.
    {
        APC_WQ_PreallocateRegQueue(tgid, pid);
    }

    // get or allocate array
    // map<task pid, dynamic array<ODEWorkhandler *>>
    {
        if (NO_ERROR(err = chain_get(work_queues, tgid, nullptr, (void **)&pchain)))
        {
            chain_p chain = *pchain;
            dyn_list_head_p * plisthead;

            if (NO_ERROR(err = chain_get(chain, pid, nullptr, (void **)&plisthead)))
            {
                pidHandle = nullptr;
                listhead = *plisthead;
            }
            else if (err == kErrorLinkNotFound)
            {
                if (ERROR(err = APC_WQ_RegisterPID_s(chain, pid, listhead, pidHandle)))
                {
                    return err;
                }
            }
            else if (ERROR(err))
            {
                return err;
            }
            else
            {
                ASSERT(false, "illegal logic state");
            }
        }
        else if (err == kErrorLinkNotFound)
        {
            chain_p chain;
            link_p link;

            if (ERROR(err = APC_WQ_RegisterTGID_s(tgid, chain, link)))
            {
                return err;
            }

            if (ERROR(err = APC_WQ_RegisterPID_s(chain, pid, listhead, pidHandle)))
            {
                chain_deallocate_handle(link); // we don't actually need this. cleaned up on process exist
                return err;
            }
        }
        else if (ERROR(err))
        {
            return err;
        }
        else
        {
            ASSERT(false, "illegal logic state");
        }
    }

    // Append work entry to list 
    {
        ODEWorkHandler ** pimpl;
        
        err = dyn_list_append(listhead, (void **)&pimpl);
        if (ERROR(err))
        {
            if (pidHandle) // destory handle, if we created it.
                chain_deallocate_handle(pidHandle);

            return err;
        }

        *pimpl = impl;
    }

    // Boot task, if need be.
    return ERROR(APC_WQ_TrySchedule_s(tsk, impl, listhead)) ? kFuckMe : kStatusOkay;
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
        if (ERROR(dyn_list_get_by_index(*listhead, 0, (void **)&pimpl)))
        {
            LogPrint(kLogError, "6: APC_PopComplete_s failed... shit. TODO: Reece: how should we handle this?");
            return;
        }

        next = *pimpl;
    }
}

static void APC_FreeThreadStack_s(void * data)
{
    error_t er;
    chain_p chain = *(chain_p *)data;

    er = chain_iterator(chain, [](uint64_t hash, void * buffer, void * ctx) {
        error_t    err;
        APCStack * stack = (APCStack *)buffer;

        APC_ReleaseStack_s(stack);

    }, nullptr);
    ASSERT(NO_ERROR(er), "iteration failure. Code: 0x%zx", er);

    ASSERT(NO_ERROR(er = chain_destory(chain)), "error 0x%zx", er);
}

static void APC_FreeThreadRestore(void * data)
{
    error_t er;
    chain_p chain = *(chain_p *)data;

    ASSERT(NO_ERROR(er = chain_destory(chain)), "error 0x%zx", er);
}

static void APC_FreeWorkHandlers(void * data)
{
    error_t er;
    chain_p chain = *(chain_p *)data;

    er = chain_iterator(chain, [](uint64_t hash, void * buffer, void * ctx) {
        error_t err;
        dyn_list_head_p *  listhead = (dyn_list_head_p *)buffer;
        dyn_list_head_p    list     = *listhead;

        err = dyn_list_iterate(list, [](void * buffer, void * ctd)
        {
            ODEWorkHandler **  listitem = (ODEWorkHandler **)buffer;
            ODEWorkHandler *   listvalue = *listitem;
            LogPrint(kLogDbg, "APC: prematurely destorying work handler %p\n", listvalue);

            listvalue->Die();
        }, nullptr);
        ASSERT(NO_ERROR(err), "[lambda] iteration failure. Code: 0x%zx", err);
    
        dyn_list_destory(*listhead);
        LogPrint(kLogDbg, "APC: destoryed work handler list\n");
    }, nullptr);
    ASSERT(NO_ERROR(er), "iteration failure. Code: 0x%zx", er);

    er =  chain_destory(chain);
    ASSERT(NO_ERROR(er), "error 0x%zx", er);
}

static void APC_CleanupTask_s(task_k tsk)
{
    link_p link;
    dyn_list_head_p * listhead;
    uint_t pid;
    uint_t tgid;
    void * entry;

    pid  = ProcessesGetPid(tsk);
    tgid = ProcessesGetTgid(tsk);

    if (pid != tgid)
    {
        LogPrint(kLogDbg, "%x (%i) isn't thread leader %i", tsk, pid, tgid);
        return;
    }

    LogPrint(kLogDbg,  "cleaning up process %p (%i)", tsk, pid, tgid);

    if (NO_ERROR(chain_get(work_process_ips, tgid, &link, nullptr)))
        chain_deallocate_handle(link);

    if (NO_ERROR(chain_get(work_thread_stacks, tgid, &link, &entry)))
    {
        APC_FreeThreadStack_s(entry);
        chain_deallocate_handle(link);
    }

    if (NO_ERROR(chain_get(work_restore, tgid, &link, &entry)))
    {
        APC_FreeThreadRestore(entry);
        chain_deallocate_handle(link);
    }

    if (NO_ERROR(chain_get(work_queues, tgid, &link, &entry)))
    {
        APC_FreeWorkHandlers(entry);
        chain_deallocate_handle(link);
    }
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

static RegRestoreQueue * APC_WS_GetItem(uint_t tgid, uint_t pid)
{
    error_t    err;
    chain_p  *pchain;
    RegRestoreQueue  *queue;

    if (ERROR(err = chain_get(work_restore, tgid, nullptr, (void **)&pchain)))
    {
        panic("APC_TryStoreSave_s: work restore item doesn't exist for thread group leader.");
    }

    if (ERROR(err = chain_get(*pchain, pid, nullptr, (void **)&queue)))
    {
        panic("APC_TryStoreSave_s: work restore item doesn't exist for thread..");
    }

    return queue;
}

static void APC_TryStoreSave_s(task_k task, pt_regs * restore)
{
    uint_t     pid;
    uint_t     tgid;
    RegRestoreQueue  *pqueue;

    tgid = ProcessesGetTgid(task);
    pid  = ProcessesGetPid(task);

    pqueue = APC_WS_GetItem(tgid, pid);

    if (!pqueue->hasPreviousTask)
    {
        pqueue->hasPreviousTask = true;
        pqueue->restore = *restore;
    }
}

static void APC_RestoreUserState_s(task_k task, pt_regs * fallback)
{
    uint_t     pid;
    uint_t     tgid;
    RegRestoreQueue  *pqueue;
    pt_regs     regs;

    tgid = ProcessesGetTgid(task);
    pid  = ProcessesGetPid(task);

    pqueue = APC_WS_GetItem(tgid, pid);

    if (pqueue->hasPreviousTask)
    {
        regs = pqueue->restore;
        pqueue->hasPreviousTask = false;

        LogPrint(kLogDbg, "APC: returning to usermode using stored registers - we had to deal with multiple items within the queue. RIP: %p", regs.rip);
        APC_PreemptThread_s(task, &regs, false);
    }
    else
    {
        LogPrint(kLogDbg, "APC: returning to usermode without using stored registers - the queue should be emtpy. RIP: %p", fallback->rip);
        APC_PreemptThread_s(task, fallback, false);
    }
}

static void APC_Complete_s(task_k task, size_t ret)
{
    bool moreWorkPending;
    ODEWorkHandler * next;
    ODEWorkHandler * cur;

    next = nullptr;

    APC_PopComplete_s(task, cur, moreWorkPending, next);

    cur->Hit(ret);

    if (moreWorkPending)
    {
        APC_TryStoreSave_s(task, &GetInfoForTask(task)->previous_user);
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

    APC_CleanupTask_s(task);
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

static error_t APC_AddPendingWork(task_k tsk, ODEWorkHandler * impl)
{
    error_t err;
    mutex_lock(work_mutex);
    err = APC_AddPendingWork_s(tsk, impl);
    mutex_unlock(work_mutex);
    return err;
}

static void InitReturnStub_64()
{
    const uint8_t x86_64[] = { 0x48, 0xC7, 0xC7, 0x05, 0x00, 0x00, 0x00, 0x48, 0x89, 0xC6, 0x48, 0xC7, 0xC0, 0x90, 0x01, 0x00, 0x00, 0x0F, 0x05 };

    size_t addr;
    error_t err;
    OLVirtualAddressSpace * vas;
    OLMemoryAllocation * alloc;
    OLPageEntry entry;

    err = g_memory_interface->GetKernelAddressSpace(OUncontrollableRef<OLVirtualAddressSpace>(vas));
    ASSERT(NO_ERROR(err), "fatal error: couldn't get kernel address space interface: %zx", err);

    err = vas->NewDescriptor(0, 1, OOutlivableRef<OLMemoryAllocation>(alloc));
    ASSERT(NO_ERROR(err), "fatal error: couldn't allocate kernel address VM area: %zx", err);

    work_returnstub_64 = alloc_pages_current(GFP_KERNEL, 0);
    ASSERT(work_returnstub_64, "ODE: InitReturnStub_64, couldn't allocate return stub");

    entry.meta = g_memory_interface->CreatePageEntry(OL_ACCESS_READ | OL_ACCESS_WRITE, kCacheNoCache);
    entry.type = kPageEntryByPage;
    entry.page = work_returnstub_64;

    err = alloc->PageInsert(0, entry);
    ASSERT(NO_ERROR(err), "fatal error: couldn't insert page into kernel: %zx", err);

    memcpy((void *)alloc->GetStart(), x86_64, sizeof(x86_64));
}


void InitDeferredCalls()
{
    ProcessesAddExitHook(APC_OnThreadExit);

    InitReturnStub_64();

    chain_allocate(&work_queues);
    chain_allocate(&work_thread_stacks);
    chain_allocate(&work_process_ips);
    chain_allocate(&work_restore);

    work_mutex         = mutex_init();
    work_watcher_mutex = mutex_init();
}

LIBLINUX_SYM error_t CreateWorkItem(OPtr<OProcessThread> target, const OOutlivableRef<ODEWorkJob> out)
{
    error_t err;
    task_k handle;
    OPtr<OWorkQueue> wq;

    if (!target.GetTypedObject())
        return kErrorIllegalBadArgument;

    if (ERROR(err = CreateWorkQueue(1, wq)))
        return err;

    if (ERROR(err = target->GetOSHandle((void **)&handle)))
        return err;

    if (!out.PassOwnership(new ODEWorkJobImpl(handle, wq)))
        return kErrorOutOfMemory;

    return kStatusOkay;
}
