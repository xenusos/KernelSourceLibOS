/*
    Purpose: 
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#include <libos.hpp>
#include "ODEProcess.hpp"
#include "ODEThread.hpp"
#include "ODEReturn.hpp"
#include "../../Processes/OProcesses.hpp"
#include "../../Processes/OProcessHelpers.hpp"
#include "../../Memory/Linux/OLinuxMemory.hpp"
#include <Core/Utilities/OThreadUtilities.hpp>

static chain_p tgid_map;

static error_t AllocateDEThread(task_k task, chain_p chain, size_t pid, ODEImplProcess * process, ODEImplPIDThread * & thread);

ODEImplProcess::ODEImplProcess(task_k task, chain_p pids)
{
    _pids = pids;
    _task = task;

    MapReturnStub();
}

ODEImplProcess::~ODEImplProcess()
{
    error_t err;

    err = chain_destroy(_pids);
    ASSERT(NO_ERROR(err), "Couldn't free process pid tree");
}

size_t ODEImplProcess::GetReturnAddress()
{
    return _returnAddress;
}

ODEImplPIDThread * ODEImplProcess::GetOrCreateThread(task_k task)
{
    error_t err;
    size_t pid;
    ODEImplPIDThread ** handle;
    ODEImplPIDThread * thread;

    pid = ProcessesGetPid(task);

    err = chain_get(_pids, pid, NULL, reinterpret_cast<void **>(&handle));
    ASSERT((NO_ERROR(err) || (err == kErrorLinkNotFound)), "Error: " PRINTF_ERROR, err);

    if (err == kErrorLinkNotFound)
    {
        err = AllocateDEThread(task, _pids, pid, this, thread);

        if (ERROR(err))
            return nullptr;

        return thread;
    }

    thread = *handle;
    thread->UpdatePidHandle(task);
    return thread;
}

error_t ODEImplProcess::GetThread(task_k task, ODEImplPIDThread * & thread)
{
    error_t err;
    size_t pid;
    ODEImplPIDThread ** handle;

    pid = ProcessesGetPid(task);
    err = chain_get(_pids, pid, NULL, (void **)&handle);

    if (ERROR(err))
        return err;

    thread = *handle;
    thread->UpdatePidHandle(task);
    return kStatusOkay;
}

void ODEImplProcess::MapReturnStub()
{
    error_t err;
    ODumbPointer<Memory::OLVirtualAddressSpace> usrVas;
    OPtr<Memory::OLMemoryAllocation> usrAlloc;
    Memory::OLPageEntry entry;

    err = g_memory_interface->GetUserAddressSpace(_task, OOutlivableRef<Memory::OLVirtualAddressSpace>(usrVas));
    ASSERT(NO_ERROR(err), "Couldn't obtain user address space interface, error: " PRINTF_ERROR, err);

    err = usrVas->NewDescriptor(0, 1, OOutlivableRef<Memory::OLMemoryAllocation>(usrAlloc));
    ASSERT(NO_ERROR(err), "Couldn't allocate descriptor, error: " PRINTF_ERROR, err);

    entry.meta = g_memory_interface->CreatePageEntry(Memory::OL_ACCESS_READ | Memory::OL_ACCESS_EXECUTE, Memory::kCacheNoCache);
    entry.type = Memory::kPageEntryByPage;
    entry.page = DEGetReturnStub(!Utilities::Tasks::IsTask32Bit(_task));
    
    err = usrAlloc->PageInsert(0, entry);
    ASSERT(NO_ERROR(err), "Couldn't insert page into user address space " PRINTF_ERROR, err);

    usrAlloc->ForceLinger();
    _returnAddress = usrAlloc->GetStart();
}

static void DeallocateThreadByHandle(uint64_t hash, void * buffer)
{
    delete reinterpret_cast<ODEImplPIDThread *>(buffer);
}

static error_t AllocateDEThread(task_k task, chain_p chain, size_t pid, ODEImplProcess * process, ODEImplPIDThread * & thread)
{
    error_t err;
    link_p link;
    ODEImplPIDThread ** handle;

    err = chain_allocate_link(chain, pid, sizeof(ODEImplPIDThread *), DeallocateThreadByHandle, &link, (void **)&handle);
    if (ERROR(err))
        return err;

    thread = new ODEImplPIDThread(process);
    *handle = thread;

    if (!thread)
    {
        chain_deallocate_handle(link);
        return kErrorOutOfMemory;
    }

    err = thread->Init(task);
    if (ERROR(err))
    {
        chain_deallocate_handle(link);
        delete thread;
        return err;
    }

    return kStatusOkay;
}

static void DeallocateProcessByHandle(uint64_t hash, void * buffer)
{
    delete reinterpret_cast<ODEImplProcess *>(buffer);
}

static error_t AllocateDEProcess(task_k task, size_t tgid, ODEImplProcess * & out)
{
    error_t err;
    link_p link;
    ODEImplProcess ** handle;
    ODEImplProcess * proc;
    chain_p chain;

    err = chain_allocate(&chain);
    if (ERROR(err))
        return err;

    err = chain_allocate_link(tgid_map, tgid, sizeof(ODEImplProcess *), DeallocateProcessByHandle, &link, reinterpret_cast<void **>(&handle));
    if (ERROR(err))
    {
        chain_destroy(chain);
        return err;
    }

    proc = new ODEImplProcess(task, chain);
    if (!proc)
    {
        chain_destroy(chain);
        return kErrorOutOfMemory;
    }

    *handle = proc;
    out = proc;
    return kStatusOkay;
}

error_t GetDEProcess(ODEImplProcess * & out, task_k task)
{
    error_t err;
    size_t pid, tgid;
    ODEImplProcess ** handle;

    tgid = ProcessesGetTgid(task);
    pid  = ProcessesGetPid(task);

    if (pid != tgid)
        LogPrint(kLogWarning, "GetDEProcess called with a thread, not a group leader instance");

    err = chain_get(tgid_map, tgid, NULL, reinterpret_cast<void **>(&handle));

    if (err == kErrorLinkNotFound)
        return AllocateDEProcess(task, tgid, out);

    if (ERROR(err))
        return err;

    out = *handle;
    return kStatusOkay;
}

void FreeDEProcess(task_k task)
{
    error_t err;
    link_p link;
    size_t pid, tgid;
    ODEImplProcess ** handle;

    tgid = ProcessesGetTgid(task);
    pid  = ProcessesGetPid(task);

    if (pid != tgid)
        LogPrint(kLogWarning, "GetDEProcess called with a thread, not a group leader instance");

    err = chain_get(tgid_map, tgid, &link, reinterpret_cast<void **>(&handle));
    if (err == XENUS_ERROR_LINK_NOT_FOUND)
        return;
    ASSERT(NO_ERROR(err), "Couldn't free DE process object.        Error: " PRINTF_ERROR, err);

    err = chain_deallocate_handle(link);
    ASSERT(NO_ERROR(err), "Couldn't free DE process object handle. Error: " PRINTF_ERROR, err);

    delete (*handle);
}

void InitDEProcesses()
{
    error_t err;

    err = chain_allocate(&tgid_map);
    ASSERT(NO_ERROR(err), "couldn't allocate tgid map: " PRINTF_ERROR, err);
}
