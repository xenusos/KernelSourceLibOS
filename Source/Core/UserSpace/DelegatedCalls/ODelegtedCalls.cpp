/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#include <libos.hpp>

#include "ODelegtedCalls.hpp"
#include "../DeferredExecution/ODeferredExecution.hpp"
#include "../../Processes/OProcesses.hpp"

#include <Core/CPU/OThread.hpp>

static mutex_k symbol_mutex;
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

    mutex_lock(symbol_mutex);

    er = dyn_list_append(delegated_fns, (void **)&inst);
    if (ERROR(er))
    {
        mutex_unlock(symbol_mutex);
        return er;
    }
   
    memset(inst, 0, sizeof(DelegatedCallInstance_p));
    memcpy(inst->name, name, MIN(strlen(name), sizeof(inst->name) - 1));
    inst->fn = fn;

    mutex_unlock(symbol_mutex);
    return kStatusOkay;
}

static size_t DelegatedCallsGetBuffer(void * buf, size_t len)
{
    size_t cnt;
    size_t index;
    error_t err;

    index = 0;

    mutex_lock(symbol_mutex);

    err = dyn_list_entries(delegated_fns, &cnt);
    if (ERROR(err))
    {
        LogPrint(kLogError, "dyn_list_entries failed: 0x%zx. how even? wtf", err);
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
            LogPrint(kLogError, "dyn_list_get_by_index failed: 0x%zx. how even? wtf", err);
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
    mutex_unlock(symbol_mutex);
    return index;
}

static size_t DelegatedCallsHandlePullComplete(user_addr_t address, size_t length)
{
    void * temp;
    size_t ret;

    if (length > 1024 * 1024)
    {
        LogPrint(kLogWarning, "Refused insane buffer allocation");
        return 0;
    }

    temp = zalloc(length);

    if (!temp)
    {
        LogPrint(kLogWarning, "out memory - couldn't provide pull");
        return 0;
    }

    ret = DelegatedCallsGetBuffer(temp, length);

    _copy_to_user(address, temp, length);
    free(temp);

    return ret;
}

static size_t DelegatedCallsHandlePull(xenus_syscall_p atten)
{
    if (atten->arg_alpha && atten->arg_bravo)
        return DelegatedCallsHandlePullComplete((user_addr_t)atten->arg_alpha, atten->arg_bravo);

    if (!atten->arg_alpha)
        return DelegatedCallsGetBuffer(nullptr, 0);

    return -1;
}

static bool DelegatedCallsLookup(size_t id, DelegatedCallInstance_p & out)
{
    error_t err;
    DelegatedCallInstance_p fn;

    mutex_lock(symbol_mutex);

    err = dyn_list_get_by_index(delegated_fns, id, (void **)&fn);
    if (ERROR(err))
    {
        mutex_unlock(symbol_mutex);
        return false;
    }

    mutex_unlock(symbol_mutex);

    out = fn;
    return true;
}

static void DelegatedCallsInitJobContext(xenus_syscall_p atten, bool buffered, SysJob_s & job)
{
    if (buffered)
    {
        _copy_from_user(&job.attention, (user_addr_t)atten->arg_alpha, sizeof(xenus_syscall_extended_t));
        return;
    }

    job.attention.attention_id  = (uint32_t)atten->arg_alpha;
    job.attention.arg_alpha     = atten->arg_bravo;
    job.attention.arg_bravo     = atten->arg_charlie;
    job.attention.arg_charlie   = atten->arg_delta;
    job.attention.arg_delta     = atten->arg_echo;
}

static size_t DelegatedCallsHandleCall(xenus_syscall_p atten, bool noBuf)
{
    DelegatedCallInstance_p fn;
    SysJob_s job;
    error_t err;
    uint64_t ret;

    DelegatedCallsInitJobContext(atten, !noBuf, job);

    if (!DelegatedCallsLookup(job.attention.attention_id, fn))
        return 0xBEEFCA3EDEADBEEF;

    job.fn              = fn->fn;
    job.attention.task  = OSThread;

    fn->fn(&job.attention);

    return job.attention.response;
}

static void DelegatedCallsHandleDeferredExec(xenus_syscall_p atten)
{
    DeferredExecFinish(atten->arg_alpha/*, atten->arg_bravo*/);
}

void DelegatedCallsSysCallHandler(xenus_syscall_ref atten)
{
    switch (atten->attention_id)
    {
    case BUILTIN_CALL_DB_PULL:
        atten->response = DelegatedCallsHandlePull(atten);
        break;
    case BUILTIN_CALL_EXTENDED:
        atten->response = DelegatedCallsHandleCall(atten, false);
        break;
    case BUILTIN_CALL_SHORT:
        atten->response = DelegatedCallsHandleCall(atten, true);
        break;
    case BUTLTIN_CALL_NTFY_COMPLETE:
        DelegatedCallsHandleDeferredExec(atten);
        break;
    default:
        LogPrint(kLogWarning, "Couldn't execute illegal user syscall (attention id: %zu) ", atten->attention_id);
        break;
    }
}

void InitDelegatedCalls()
{
    delegated_fns = DYN_LIST_CREATE(DelegatedCallInstance_t);
    ASSERT(delegated_fns, "couldn't create dynamic list for delegated calls");

    symbol_mutex = mutex_create();
    ASSERT(symbol_mutex, "couldn't create delegated call mutex");
}
