/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#include <libos.hpp>

#include "ODelegtedCalls.hpp"
#include "OPseudoFile.hpp"
#include "ODeferredExecution.hpp"
#include "../Processes/OProcesses.hpp"

#include <Core/CPU/OThread.hpp>

static mutex_k delegated_mutex;
static dyn_list_head_p delegated_fns;

typedef struct SysJob_s
{
    xenus_attention_syscall_t attention;
    DelegatedCall_t fn;
} SysJob_t, *SysJob_ref, *SysJob_p;

typedef struct DelegatedCallInstance_s
{
    DelegatedCall_t fn;
    char name[100];
} DelegatedCallInstance_t, *DelegatedCallInstance_p;

error_t AddKernelSymbol(const char * name, DelegatedCall_t fn)
{
    error_t er;
    DelegatedCallInstance_p inst;

    if (!name)
        return kErrorIllegalBadArgument;

    if (!fn)
        return kErrorIllegalBadArgument;

    mutex_lock(delegated_mutex);
    
    if (ERROR(er = dyn_list_append(delegated_fns, (void **)&inst)))
    {
        mutex_unlock(delegated_mutex);
        return er;
    }
   
    memset(inst, 0, sizeof(DelegatedCallInstance_p));
    memcpy(inst->name, name, MIN(strlen(name), sizeof(inst->name) - 1));
    inst->fn = fn;

    mutex_unlock(delegated_mutex);
    return kStatusOkay;
}

size_t DelegatedCallsGetBuffer(void * buf, size_t len)
{
    size_t cnt;
    size_t index;
    error_t err;

    index = 0;

    mutex_lock(delegated_mutex);

    if (ERROR(err = dyn_list_entries(delegated_fns, &cnt)))
    {
        LogPrint(kLogError, "dyn_list_entries failed: %lli. how even? wtf", err);
        goto exit;
    }

    ASSERT(cnt < UINT32_MAX, "too many allocated syscalls");

    for (uint32_t i = 0; i < uint32_t(cnt); i++)
    {
        size_t nlen;
        DelegatedCallInstance_p fn;
        
        if (ERROR(err = dyn_list_get_by_index(delegated_fns, i, (void **)&fn)))
        {
            LogPrint(kLogError, "dyn_list_get_by_index failed: %lli. how even? wtf", err);
            goto exit;
        }
        
        nlen = strlen(fn->name);

        if (buf)
            if (nlen + 1 + sizeof(uint32_t) + index > len)
                goto exit;

        if (buf)
            memcpy((void *)(size_t(buf) + index), fn->name, nlen + 1);
        index += nlen + 1;

        if (buf)
            *(uint32_t *)(size_t(buf) + index) = i;
        index += sizeof(uint32_t);
    }

exit:
    mutex_unlock(delegated_mutex);
    return index;
}

void DelegatedCallsHandlePull(xenus_syscall_p atten)
{
    if (atten->arg_alpha && atten->arg_bravo)
    {
        size_t len;
        void * temp;

        if (atten->arg_bravo > 1024 * 1024)
        {
            LogPrint(kLogWarning, "nice try. no crash today");
            return;
        }

        len = atten->arg_bravo;
        temp = zalloc(len);

        atten->response = DelegatedCallsGetBuffer(temp, len);

        _copy_to_user((user_addr_t)atten->arg_alpha, temp, len);
        free(temp);
    }
    else if (atten->arg_alpha)
    {
        LogPrint(kLogWarning, "Illegal deferred call... unofficial LibInterRingComms?");
    }
    else if (!atten->arg_alpha)
    {
        atten->response = DelegatedCallsGetBuffer(NULL, 0);
    }
}

void DelegatedCallsHandleCall(xenus_syscall_p atten)
{
    DelegatedCallInstance_p fn;
    SysJob_s job;
    error_t err;
    uint64_t ret;

    _copy_from_user(&job.attention, (user_addr_t)atten->arg_alpha, sizeof(xenus_syscall_extended_t));

    mutex_lock(delegated_mutex);
    if (ERROR(dyn_list_get_by_index(delegated_fns, job.attention.attention_id, (void **)&fn)))
    {
        mutex_unlock(delegated_mutex);
        LogPrint(kLogWarning, "Couldn't execute user syscall (attention id: %zu)", atten->attention_id);
        return;
    }
    mutex_unlock(delegated_mutex);

    job.fn = fn->fn;                // stack size, i think, was a massive issue - defer work to a thread pool
    job.attention.task = OSThread;

    fn->fn(&job.attention);

    _copy_to_user((user_addr_t)atten->arg_alpha, &job.attention, sizeof(xenus_syscall_extended_t));
    atten->response = 0;
}

void DelegatedCallsHandleDeferredExec(xenus_syscall_p atten)
{
    DeferredExecFinish(atten->arg_alpha/*, atten->arg_bravo*/);
}

void DelegatedCallsSysCallHandler(xenus_syscall_ref atten)
{
    puts("Syscall handler in");
    if (atten->attention_id == BUILTIN_CALL_DB_PULL)
    {
        DelegatedCallsHandlePull(atten);
    }
    else if (atten->attention_id == BUILTIN_CALL_EXTENDED)
    {
        DelegatedCallsHandleCall(atten);
    }
    else if (atten->attention_id == BUTLTIN_CALL_NTFY_COMPLETE)
    {
        DelegatedCallsHandleDeferredExec(atten);
    }
    else
    {
        LogPrint(kLogWarning, "Couldn't execute illegal user syscall (attention id: %zu) ", atten->attention_id);
    }
}

void InitDelegatedCalls()
{
    delegated_fns = DYN_LIST_CREATE(DelegatedCallInstance_t);
    ASSERT(delegated_fns, "couldn't create dynamic list for delegated calls");

    delegated_mutex = mutex_create();
    ASSERT(delegated_mutex, "couldn't create delegated call mutex");
}
