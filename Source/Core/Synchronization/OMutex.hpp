/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#pragma once
#include <Core/Synchronization/OMutex.hpp>

class OMutexImpl : public Synchronization::OMutex
{
public:
    OMutexImpl(mutex_k mutex);

    void Lock()          override;
    void Unlock()        override;

private:
    void InvalidateImp() override;

private:
    mutex_k _mutex;
};

LIBLINUX_SYM error_t Synchronization::CreateMutex(const OOutlivableRef<Synchronization::OMutex> & out);
