/*
    Purpose: hacky process tracking stuff (TODO: revisit this)
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#include <libos.hpp>
#include "OProcesses.hpp"
#include "OProcessHelpers.hpp"
#include "OProcessTracking.hpp"

#include <Core/CPU/OThread.hpp>

static mutex_k tracking_mutex;
static linked_list_head_p tracking_exit_cbs;
static linked_list_head_p tracking_start_cbs;
static chain_p tracking_locked;

static void ProcessesExit(long exitcode)
{
    link_p link;
    linked_list_entry_p entry;
    error_t err;
    OProcess * proc;

    proc = new OProcessImpl(OSThread);
    if (!proc)
    {
        LogPrint(kLogWarning, "Processes hack: out of memory - not ntfying process exit");
        return;
    }

    mutex_lock(tracking_mutex);
   
    err = chain_get(tracking_locked, thread_geti(), &link, NULL);
    if (NO_ERROR(err))
    {
        chain_deallocate_handle(link);
   
        for (linked_list_entry_p cur = tracking_exit_cbs->bottom; cur != NULL; cur = cur->next)
        {
            (*(ProcessExitNtfy_cb*)(cur->data))(OPtr<OProcess>(proc));
        }
    }
   
    mutex_unlock(tracking_mutex);

    proc->Destroy();
}

static void ProcessesStart(task_k tsk)
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

    proc->Destroy();
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

static void ProcessTrackingInitExitHandler(task_k task)
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
            panic("couldn't install thread exit callback for libos process tracking");
        }
    }

    cb_arr[i] = ProcessesExit;
}

static void ProcessesRegisterTsk(task_k tsk)
{
    ProcessesStart(tsk);
    ProcessTrackingInitExitHandler(tsk);
    //threading_ntfy_singleshot_exit(ProcessesGetPid(tsk), ProcessesExit);
}

static void ProcessesTryRegister(task_k tsk)
{
    error_t er;
    
    mutex_lock(tracking_mutex);
    er = chain_allocate_link(tracking_locked, ProcessesGetPid(tsk), 2, NULL, NULL, NULL);
    mutex_unlock(tracking_mutex);

    ASSERT(NO_ERROR(er), "couldn't register thread pid / ProcessesRegisterTsk");

    if (er != kStatusLinkPresent)
        ProcessesRegisterTsk(tsk);
}

void ProcessesTryRegisterLeader(task_k tsk)
{
    ProcessesTryRegister(ProcessesGetProcess(tsk));
}

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
