/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/  
#pragma once
#include <Core\FIO\ODirectory.hpp>

class OLinuxPathImpl;
class ODirectoryImp : public IO::ODirectory
{
public:
    ODirectoryImp(path_k path);
    ODirectoryImp(vfsmount_k mnt, dentry_k entry);
    error_t Iterate(void(*iterator)(ODirectory * directory, const char * file, void * data), void * _ctx)   override;
    error_t Stat(const OOutlivableRef<IO::OFileStat> & stat)                                                override;
    error_t Rename(const char * path)                                                                       override;
    error_t GetPath(const char ** path)                                                                     override;
    error_t UpDir(const OOutlivableRef<ODirectory> & dir)                                                   override;
    error_t Delete()                                                                                        override;
    error_t Lock()                                                                                          override;
    error_t Unlock()                                                                                        override;
    error_t LockState(bool & state)                                                                         override;

protected:                                                                                                  
    void InvalidateImp()                                                                                    override;

private:
    OLinuxPathImpl * _path;
    inode_k _inode;
    char _cpath[256];
    bool _locked;
};

LIBLINUX_SYM error_t   IO::CreateDirectory(const OOutlivableRef<IO::ODirectory> & dir, const char * path, umode_t mode);
LIBLINUX_SYM error_t   IO::OpenDirectory(const OOutlivableRef<IO::ODirectory> & dir, const char * path);
LIBLINUX_SYM error_t   IO::OpenDirectory(const OOutlivableRef<IO::ODirectory> & dir, vfsmount_k mnt, dentry_k entry);
