/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/  
#pragma once
#include <Core\FIO\OPath.hpp>
#include <ITypes\IVFSMount.hpp> //private header; i don't care
#include <ITypes\IDEntry.hpp>
#include <ITypes\IPath.hpp>

class OLinuxPathImpl : public IO::OPath
{
public:
    OLinuxPathImpl(vfsmount_k mnt, dentry_k dentry);

    bool        IsEqualTo(const IO::OPath * path)        override;
    uint_t      ToString(char * str, uint_t length) override;

    dentry_k    GetDEntry();
    vfsmount_k  GetMount();
    
    inode_k     ToINode();
    void        ToPathPtr(path_k path);

    error_t     GetParent(const OOutlivableRef<OLinuxPathImpl> & upper); // linux fio is a fucking pain. mostly used with files -> dirs
   // error_t     GetParent_2(const OOutlivableRef<OLinuxPathImpl> & upper); // linux fio is a fucking pain. mostly used with dirs  -> dirs

protected:
    void        InvalidateImp()    override;

private:
    vfsmount_k _mnt;
    dentry_k _dentry;
};


extern error_t OpenLinuxPath(const OOutlivableRef<OLinuxPathImpl>& out, path_k path);
extern error_t OpenLinuxPath(const OOutlivableRef<OLinuxPathImpl>& out, vfsmount_k mnt, dentry_k entry);
