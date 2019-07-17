/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/  
#include <libos.hpp>

#include <ITypes\IThreadStruct.hpp>
#include <ITypes\ITask.hpp>

#include <Utils\ThreadHelper.hpp>

namespace ThreadHelpers
{
    uint32_t GetAddressLimit()
    {
        return ITask(OSThread).GetAddressLimit();
    }

    uint32_t SwapAddressLimit(uint32_t fs)
    {
        return ITask(OSThread).SwapAddressLimit(fs);
    }

    void SetAddressLimitUnsafe(uint32_t fs)
    {
        ITask(OSThread).SetAddressLimitUnsafe(fs);
    }
}