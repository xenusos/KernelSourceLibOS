/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#pragma once

class OMutex : public  OObject
{
public:
    virtual void Lock()   = 0;
    virtual void Unlock() = 0;
};

LIBLINUX_SYM error_t CreateMutex(const OOutlivableRef<OMutex> & mutex);