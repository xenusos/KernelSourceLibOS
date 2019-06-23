/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#include <libos.hpp>

#include "ODelegtedCalls.hpp"
#include "../DeferredExecution/ODeferredExecution.hpp"
#include "../../Processes/OProcesses.hpp"

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

static size_t DelegatedCallsGetBuffer(void * buf, size_t len)
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
        
        err = dyn_list_get_by_index(delegated_fns, i, (void **)&fn);
        if (ERROR(err))
        {
            LogPrint(kLogError, "dyn_list_get_by_index failed: %lli. how even? wtf", err);
            goto exit;
        }
        
        nlen = strlen(fn->name) + 1;

        if (buf)
            if (nlen + sizeof(uint32_t) + index > len)
                goto exit;

        if (buf)
            memcpy((void *)(size_t(buf) + index), fn->name, nlen);
        index += nlen;

        if (buf)
            *(uint32_t *)(size_t(buf) + index) = i;
        index += sizeof(uint32_t);
    }

exit:
    mutex_unlock(delegated_mutex);
    return index;
}

static size_t DelegatedCallsHandlePull(xenus_syscall_p atten)
{
    if (atten->arg_alpha && atten->arg_bravo)
    {
        size_t len;
        void * temp;
        size_t ret;

        if (atten->arg_bravo > 1024 * 1024)
        {
            LogPrint(kLogWarning, "nice try. no crash today");
            return 0;
        }

        len = atten->arg_bravo;
        temp = zalloc(len);

        if (!temp)
        {
            LogPrint(kLogWarning, "out memory - couldn't provide pull");
            return 0;
        }

        ret = DelegatedCallsGetBuffer(temp, len);

        _copy_to_user((user_addr_t)atten->arg_alpha, temp, len);
        free(temp);
    
        return ret;
    }
    
    if (atten->arg_alpha)
    {
        LogPrint(kLogWarning, "Illegal deferred call... unofficial LibInterRingComms?");
        return 0;
    }
    
    if (!atten->arg_alpha)
    {
        return DelegatedCallsGetBuffer(NULL, 0);
    }

    return -1;
}

static size_t DelegatedCallsHandleCall(xenus_syscall_p atten, bool noBuf)
{
    DelegatedCallInstance_p fn;
    SysJob_s job;
    error_t err;
    uint64_t ret;

    if (noBuf)
    {
        job.attention.attention_id = atten->arg_alpha;
        job.attention.arg_alpha = atten->arg_bravo;
        job.attention.arg_bravo = atten->arg_charlie;
        job.attention.arg_charlie = atten->arg_delta;
        job.attention.arg_delta = atten->arg_echo;
    }
    else 
    {
        _copy_from_user(&job.attention, (user_addr_t)atten->arg_alpha, sizeof(xenus_syscall_extended_t));
        // TODO: check response
    }

    mutex_lock(delegated_mutex);
    err = dyn_list_get_by_index(delegated_fns, job.attention.attention_id, (void **)&fn);
    if (ERROR(err))
    {
        mutex_unlock(delegated_mutex);
        LogPrint(kLogWarning, "Couldn't execute user syscall (attention id: %zu)", atten->attention_id);
        return 0xDEAFBEEFDEADBEEF;
    }
    mutex_unlock(delegated_mutex);

    job.fn = fn->fn;
    job.attention.task = OSThread;

    fn->fn(&job.attention);

    // [GREP FOR ME = SYSCALL RETURN CHANGE]
    //_copy_to_user((user_addr_t)atten->arg_alpha, &job.attention, sizeof(xenus_syscall_extended_t));
    return job.attention.response;
}

static void DelegatedCallsHandleDeferredExec(xenus_syscall_p atten)
{
    DeferredExecFinish(atten->arg_alpha/*, atten->arg_bravo*/);
}

void DelegatedCallsSysCallHandler(xenus_syscall_ref atten)
{
    if (atten->attention_id == BUILTIN_CALL_DB_PULL)
    {
        atten->response = DelegatedCallsHandlePull(atten);
    }
    else if (atten->attention_id == BUILTIN_CALL_EXTENDED)
    {
        atten->response = DelegatedCallsHandleCall(atten, false);
    }
    else if (atten->attention_id == BUTLTIN_CALL_NTFY_COMPLETE)
    {
        DelegatedCallsHandleDeferredExec(atten);
    }
    else if (atten->attention_id == BUILTIN_CALL_SHORT)
    {
        atten->response = DelegatedCallsHandleCall(atten, true);
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
