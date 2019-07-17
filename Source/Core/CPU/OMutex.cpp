/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/  
#include <libos.hpp>
#include "OMutex.hpp"

OMutexImpl::OMutexImpl(mutex_k mutex)
{
    _mutex = mutex;
}

void OMutexImpl::Lock()
{
    mutex_lock(_mutex);
}

void OMutexImpl::Unlock()
{
    mutex_unlock(_mutex);
}

void OMutexImpl::InvalidateImp()
{
    if (_mutex)
        mutex_destroy(_mutex);
}

error_t CreateMutex(const OOutlivableRef<OMutex> & out)
{
    mutex_k mutex;

    mutex = (mutex_k) mutex_init();

    if (!mutex)
        return kErrorInternalError;

    if (!(out.PassOwnership(new OMutexImpl(mutex))))
        return kErrorOutOfMemory;

    return kStatusOkay;
}
