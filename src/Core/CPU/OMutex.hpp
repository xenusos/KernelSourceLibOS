#pragma once

#include <Core/CPU/OMutex.hpp>

class OMutexImpl : public OMutex
{
public:
    OMutexImpl(mutex_k mutex);

    void Lock()   override;
    void Unlock() override;

private:
    void InvaildateImp() override;
private:
    mutex_k _mutex;
};
