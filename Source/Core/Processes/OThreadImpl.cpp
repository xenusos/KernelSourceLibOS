/*
    Purpose: Generic process API built on top of linux
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/

#include <libos.hpp>
#include "OProcesses.hpp"
#include "OThreadImpl.hpp"
#include "OProcessHelpers.hpp"


OProcessThreadImpl::OProcessThreadImpl(task_k tsk, bool isProc, OProcess * process)
{
    ProcessesTaskIncrementCounter(tsk);
    _tsk = tsk;
    _id = ProcessesGetPid(tsk);
    _process = process;
    _isProc = isProc;

    memcpy(_name, task_get_comm(_tsk), THREAD_NAME_LEN);
    _name[strnlen(_name, THREAD_NAME_LEN)] = 0;
}

void OProcessThreadImpl::InvalidateImp()
{
    ProcessesTaskDecrementCounter(_tsk);
}

error_t OProcessThreadImpl::GetThreadName(const char ** name)
{
    CHK_DEAD;

    if (!name)
        return kErrorIllegalBadArgument;

    *name = _name;
    return kStatusOkay;
}

error_t OProcessThreadImpl::GetOSHandle(void ** handle)
{
    CHK_DEAD;

    if (!handle)
        return kErrorIllegalBadArgument;

    *handle = _tsk;
    return kStatusOkay;
}

error_t OProcessThreadImpl::GetId(uint_t * id)
{
    CHK_DEAD;

    if (!id)
        return kErrorIllegalBadArgument;

    *id = _id;
    return kStatusOkay;
}

error_t OProcessThreadImpl::GetParent(OUncontrollableRef<OProcess> parent)
{
    CHK_DEAD;
    parent.SetObject(_process);
    return kStatusOkay;
}

error_t OProcessThreadImpl::IsProcess(bool * out)
{
    CHK_DEAD;

    if (!out)
        return kErrorIllegalBadArgument;

    *out = _isProc;
    return kStatusOkay;
}

error_t OProcessThreadImpl::AsProcess(const OOutlivableRef<OProcess> parent)
{
    CHK_DEAD;

    if (!_isProc)
        return kErrorProcessPidInvalid;

    if (!parent.PassOwnership(new OProcessImpl(_tsk)))
        return kErrorOutOfMemory;

    return kStatusOkay;
}
