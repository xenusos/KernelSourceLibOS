/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/  
#include <libos.hpp>
#include "OSpinlock.hpp"

Synchronization::Spinlock::Spinlock() : _value(0) 
{}

void Synchronization::Spinlock::Lock()
{
    while (_interlockedbittestandset(&_value, 0))
    {
        while (_value)
        {
            SPINLOOP_PROCYIELD();
        }
    }
}

void Synchronization::Spinlock::Unlock()
{
    _value = 0;
}

bool Synchronization::Spinlock::IsLocked()
{
    return _value ? true : false;
}
