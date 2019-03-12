/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson
*/  
#include <libos.hpp>
#include <ITypes\IPath.hpp>

#include "OPath.hpp"

OLinuxPathImpl::OLinuxPathImpl(vfsmount_k mnt, dentry_k dentry)
{
    _dentry    = dentry;
    _mnt    = mnt;
    
    lockref_get((lockref_k)dentry_get_d_lockref(_dentry)); //__dget
    mntget(_mnt);                                          //mntget
}

void OLinuxPathImpl::InvalidateImp()
{
    dput(_dentry);
    mntput(_mnt);
}

dentry_k OLinuxPathImpl::GetDEntry()
{
    return _dentry;
}

vfsmount_k OLinuxPathImpl::GetMount()
{
    return _mnt;
}

inode_k OLinuxPathImpl::ToINode()
{
    return (inode_k) dentry_get_d_inode_uint64(_dentry);
}

void OLinuxPathImpl::ToPathPtr(path_k path)
{
    IPath writable(path);
    writable.GetVarDEntry().SetUInt((uint_t)_dentry);
    writable.GetVarMount().SetUInt((uint_t)_mnt);
}

uint_t OLinuxPathImpl::ToString(char * str, uint_t length)
{
    path_k ahh;
    char * ffs;
    uint32_t len;

    ahh = alloca(path_size());
    this->ToPathPtr(ahh);

    ffs = d_path(ahh, str, (l_int)length);
    
    if (!((((uint_t)ffs) >= ((uint_t)str)) && (((uint_t)ffs) < ((uint_t)str + length))))
        return -1;
    
    len = ((uint_t)str + length - (uint_t)ffs);
    memmove(str, ffs, len);
    str[length - 1] = '\x00'; 
    //TODO Reece: check for exploits / underflows / whatevers
    return length;
}

bool OLinuxPathImpl::IsEqualTo(const OPath * path)
{
    return ((OLinuxPathImpl *)path)->_dentry == this->_dentry;
}

error_t OLinuxPathImpl::GetParent_1(const OOutlivableRef<OLinuxPathImpl> & out)
{
    error_t err;
    dentry_k parent;

    err = kStatusOkay;

    if (!(parent = dget_parent(_dentry)))
        return kErrorInternalError;

    if (!(out.PassOwnership(new OLinuxPathImpl(_mnt, parent))))
        err = kErrorOutOfMemory;

    dput(parent);
    return err;
}

error_t OLinuxPathImpl::GetParent_2(const OOutlivableRef<OLinuxPathImpl> & out)
{
    error_t err;
    dentry_k parent;

    err = kStatusOkay;

    if (!(parent = (dentry_k)(IDEntry(_dentry).GetVarParent().GetUInt())))
        return kErrorInternalError;

    if (!(out.PassOwnership(new OLinuxPathImpl(_mnt, parent))))
        err = kErrorOutOfMemory;

    return err;
}

error_t OpenLinuxPath(const OOutlivableRef<OLinuxPathImpl>& out, path_k path)
{
    IPath readablePath(path);

    if (!path)
        return kErrorIllegalBadArgument;

    return OpenLinuxPath(out, readablePath.GetMount(), readablePath.GetDEntry());
}

error_t OpenLinuxPath(const OOutlivableRef<OLinuxPathImpl>& out, vfsmount_k mnt, dentry_k entry)
{
    if (!mnt)
        return kErrorIllegalBadArgument;

    if (!entry)
        return kErrorIllegalBadArgument;

    if (!(out.PassOwnership(new OLinuxPathImpl(mnt, entry))))
        return kErrorOutOfMemory;

    return kStatusOkay;
}
