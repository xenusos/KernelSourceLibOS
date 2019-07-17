/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/  
#pragma once

#if defined(TGT_KRN_LINUX)
    // unix mode
    #define DT_UNKNOWN    0
    #define DT_FIFO       1
    #define DT_CHR        2
    #define DT_DIR        4
    #define DT_BLK        6
    #define DT_REG        8
    #define DT_LNK        10
    #define DT_SOCK       12
    #define DT_WHT        14
#endif


class OFileStat : public OObject
{
public:
    virtual uint64_t GetModifiedTime()  = 0;
    virtual uint64_t GetCreationTime()  = 0;
    virtual uint64_t GetAccessedTime()  = 0;
    virtual uint64_t GetFileLength()    = 0;
    virtual uint64_t GetUNIXMode()      = 0;
    virtual uint64_t GetUserID()        = 0;
    virtual void * LinuxDevice()        = 0;
    virtual bool IsDirectory()          = 0;
    virtual bool IsFile()               = 0;
    virtual bool IsIPC()                = 0;
};
