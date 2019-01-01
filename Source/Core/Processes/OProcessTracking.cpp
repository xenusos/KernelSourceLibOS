/*
    Purpose: hacky process tracking stuff
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#include <libos.hpp>
#include "OProcesses.hpp"
#include "OProcessTracking.hpp"

#include <Core/CPU/OThread.hpp>

mutex_k tracking_mutex;
linked_list_head_p tracking_exit_cbs;
linked_list_head_p tracking_start_cbs;
chain_p tracking_locked;

void ProcessesExit(long exitcode)
{
    link_p link;
    linked_list_entry_p entry;
    OProcess * proc;

    proc = new OProcessImpl(OSThread);

    mutex_lock(tracking_mutex);

    if (chain_get(tracking_locked, thread_geti(), &link, NULL) == kStatusOkay)
        chain_deallocate_handle(link);

    if (!proc)
    {
        LogPrint(kLogWarning, "Processes hack: out of memory - not ntfying process exit");
        mutex_unlock(tracking_mutex);
        return;
    }

    for (linked_list_entry_p cur = tracking_exit_cbs->bottom; cur != NULL; cur = cur->next)
    {
        (*(ProcessExitNtfy_cb*)(cur->data))(OPtr<OProcess>(proc));
    }
    mutex_unlock(tracking_mutex);

    proc->Destory();
}

void ProcessesStart(task_k tsk)
{
    OProcess * proc;

    proc = new OProcessImpl(tsk);

    if (!proc)
    {
        LogPrint(kLogWarning, "Processes hack: out of memory - not ntfying process start");
        return;
    }

    mutex_lock(tracking_mutex);
    for (linked_list_entry_p cur = tracking_start_cbs->bottom; cur != NULL; cur = cur->next)
    {
        (*(ProcessStartNtfy_cb*)(cur->data))(OPtr<OProcess>(proc));
    }
    mutex_unlock(tracking_mutex);

    proc->Destory();
}

error_t ProcessesAddExitHook(ProcessExitNtfy_cb cb)
{
    linked_list_entry_p entry;
    error_t ret;

    ret = kStatusOkay;
    mutex_lock(tracking_mutex);

    entry = linked_list_append(tracking_exit_cbs, sizeof(ProcessExitNtfy_cb));
    if (!entry)
    {
        ret = kErrorOutOfMemory;
        goto exit;
    }

    *(ProcessExitNtfy_cb*)(entry->data) = cb;

exit:
    mutex_unlock(tracking_mutex);
    return ret;
}

error_t ProcessesAddStartHook(ProcessStartNtfy_cb cb)
{
    linked_list_entry_p entry;
    error_t ret;

    ret = kStatusOkay;
    mutex_lock(tracking_mutex);

    entry = linked_list_append(tracking_start_cbs, sizeof(ProcessStartNtfy_cb));
    if (!entry)
    {
        ret = kErrorOutOfMemory;
        goto exit;
    }

    *(ProcessStartNtfy_cb*)(entry->data) = cb;

exit:
    mutex_unlock(tracking_mutex);
    return ret;
}

error_t ProcessesRemoveExitHook(ProcessExitNtfy_cb cb)
{
    linked_list_entry_p entry;
    error_t ret;

    ret = kErrorCallbackNotFound;
    mutex_lock(tracking_mutex);

    for (linked_list_entry_p cur = tracking_exit_cbs->bottom; cur != NULL; cur = cur->next)
    {
        if (*(ProcessStartNtfy_cb*)(cur->data) == cb)
        {
            ret = kStatusOkay;
            linked_list_remove(cur);
            goto exit;
        }
    }


exit:
    mutex_unlock(tracking_mutex);

    return ret;
}

error_t ProcessesRemoveStartHook(ProcessStartNtfy_cb cb)
{
    linked_list_entry_p entry;
    error_t ret;

    ret = kErrorCallbackNotFound;
    mutex_lock(tracking_mutex);

    for (linked_list_entry_p cur = tracking_start_cbs->bottom; cur != NULL; cur = cur->next)
    {
        if (*(ProcessStartNtfy_cb*)(cur->data) == cb)
        {
            ret = kStatusOkay;
            linked_list_remove(cur);
            goto exit;
        }
    }

exit:
    mutex_unlock(tracking_mutex);

    return ret;
}

void ProcessesRegisterTsk(task_k tsk)
{
    ProcessesStart(tsk);

    threading_ntfy_singleshot_exit(ProcessesGetPid(tsk), ProcessesExit);
}

void ProcessesTryRegister(task_k tsk)
{
    error_t er;
    
    mutex_lock(tracking_mutex);
    ASSERT(NO_ERROR(er = chain_allocate_link(tracking_locked, ProcessesGetPid(tsk), 2, NULL, NULL, NULL)), "couldn't register thread pid / ProcessesRegisterTsk");
    mutex_unlock(tracking_mutex);

    if (er != XENUS_STATUS_LINK_ALREADY_PRESENT)
        ProcessesRegisterTsk(tsk);
}

void ProcessesTryRegisterLeader(task_k tsk)
{
    task_k leader;
    leader = (task_k)task_get_group_leader_size_t(tsk);
    ProcessesTryRegister(leader ? leader : tsk);
}


// TODO: 
// consider implementing a thread that scans for new process using public OProcess apis
// on change, check thread leader, and append handler
void InitProcessTracking()
{
    error_t err;

    tracking_mutex = mutex_create();
    ASSERT(tracking_mutex, "couldn't create thread tracking mutex");
    
    tracking_exit_cbs = linked_list_create();
    ASSERT(tracking_exit_cbs, "couldn't create tracking_exit_cbs");

    tracking_start_cbs = linked_list_create();
    ASSERT(tracking_start_cbs, "couldn't create tracking_start_cbs");

    err = chain_allocate(&tracking_locked);
    ASSERT(NO_ERROR(err), "couldn't create tracking_locked");
}