/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson
*/  
#pragma once

class OFile;
class OFileStat;
class ODirectory : public OObject
{
public:
    virtual error_t Iterate(void(*)(ODirectory * directory, const char * path, void * data), void * _ctx)       = 0;
    virtual error_t Stat(const OOutlivableRef<OFileStat> & stat)                                                = 0; 
    virtual error_t GetPath(const char ** path)                                                                 = 0;
    virtual error_t UpDir(const OOutlivableRef<ODirectory> & dir)                                               = 0;
    virtual error_t Rename(const char * path)                                                                   = 0;
    virtual error_t Delete()                                                                                    = 0;
    virtual error_t Lock()                                                                                      = 0;
    virtual error_t Unlock()                                                                                    = 0;
    virtual error_t LockState(bool & state)                                                                     = 0;
};

LIBLINUX_SYM error_t   OpenDirectory(const OOutlivableRef<ODirectory> & dir, const char * path);

#if defined(TGT_KRN_LINUX)
    LIBLINUX_SYM error_t   OpenDirectory(const OOutlivableRef<ODirectory> & dir, vfsmount_k mnt, dentry_k entry);
    LIBLINUX_SYM error_t CreateDirectory(const OOutlivableRef<ODirectory> & dir, const char * path, umode_t mode = 0777);
#endif
