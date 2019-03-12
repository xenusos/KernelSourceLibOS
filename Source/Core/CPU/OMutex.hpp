/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#pragma once
#include <Core/CPU/OMutex.hpp>

class OMutexImpl : public OMutex
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
