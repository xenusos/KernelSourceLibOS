/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/  
#include <libos.hpp>

#include <ITypes\IPath.hpp>
#include <ITypes\IDEntry.hpp>
#include <ITypes\IFile.hpp>

#include "OPath.hpp"
#include "OFile.hpp"
#include "ODirectory.hpp"
#include "OFileStat.hpp"

#include <Utils\FileIOHelper.hpp>

ODirectoryImp::ODirectoryImp(path_k path)
{
    OpenLinuxPath(OOutlivableRef<OLinuxPathImpl>(_path), path);
    _path->ToString(_cpath, 255);
    _locked = false;
}

ODirectoryImp::ODirectoryImp(vfsmount_k mnt, dentry_k entry)
{
    OpenLinuxPath(OOutlivableRef<OLinuxPathImpl>(_path), mnt, entry);
    _path->ToString(_cpath, 255);
    _locked = false;
}

void ODirectoryImp::InvalidateImp()
{
    _path->Destory();
}

error_t ODirectoryImp::GetPath(const char ** path)
{
    CHK_DEAD;

    if (!path)
        return kErrorIllegalBadArgument;
    *path = _cpath;
    return kStatusOkay;
}

error_t ODirectoryImp::UpDir(const OOutlivableRef<ODirectory> & dir)
{
    CHK_DEAD;

    error_t err;
    ODumbPointer<OLinuxPathImpl> parent;

    if (ERROR(err = _path->GetParent_1(OOutlivableRef<OLinuxPathImpl>(parent))))
        return err;

    return OpenDirectory(dir, parent->GetMount(), parent->GetDEntry());
}

error_t ODirectoryImp::Delete()
{
    CHK_DEAD;
    error_t err;
    ODumbPointer<OLinuxPathImpl> parent;

    err = _path->GetParent_1(OOutlivableRef<OLinuxPathImpl>(parent));
    if (ERROR(err))
        return err;
    
    if (vfs_rmdir(parent->ToINode(), _path->GetDEntry()) != 0)
        return kErrorInternalError;

    Invalidate();
    return kStatusOkay;
}

struct IterCtx_s
{
    void(*iterator)(ODirectory * directory, const char * file, void * data);
    void * usrctx;
    ODirectoryImp * dir;
};

DEFINE_SYSV_FUNCTON_START(dir_iter, size_t)
    dir_context_k ctx,
    const char * path,
    l_int path_length,
    loff_t path_offset,
    u64 ino,
    l_unsigned d_type,
DEFINE_SYSV_FUNCTON_END_DEF(dir_iter, size_t)
{
    IterCtx_s * ahh;
    ahh = (IterCtx_s *)SYSV_GET_DATA;

    ahh->iterator(ahh->dir, path, ahh->usrctx);

   SYSV_FUNCTON_RETURN(0)
}
DEFINE_SYSV_END

error_t ODirectoryImp::Iterate(void(* iterator)(ODirectory * directory, const char * file, void * data), void * uctx)
{
    CHK_DEAD;

    error_t ret;
    dir_context_k dctx;
    void * stubhandle;
    sysv_fptr_t stub;
    const char * path;
    file_k dirfile;
    IterCtx_s ictx;

    dctx = alloca(dir_context_size());

    ictx.usrctx   = uctx;
    ictx.iterator = iterator;
    ictx.dir      = this;

    ret = dyncb_allocate_stub(SYSV_FN(dir_iter), 6, (void *)&ictx, &stub, &stubhandle);
    if (ERROR(ret))
        return ret;

    dir_context_set_actor_uint64(dctx, uint_t(stub));

    if (ERROR(ret = GetPath(&path)))
        return ret;

    dirfile = filp_open(path, kFileDirOnly, 0777);
    
    if (LINUX_PTR_ERROR(dirfile))
        return kErrorInternalError;

    iterate_dir(dirfile, dctx);

    filp_close(dirfile, 0);
    dyncb_free_stub(stubhandle);

    return kStatusOkay;
}

error_t ODirectoryImp::Stat(const OOutlivableRef<OFileStat> & filestat)
{
    CHK_DEAD;
    path_k path;
    kstat_k stat;

    path = alloca(path_size());
    stat = alloca(kstat_size());

    _path->ToPathPtr(path);

    if (vfs_getattr(path, stat, STATX_ALL, AT_STATX_SYNC_AS_STAT) != 0)
        return kErrorInternalError;

    return CreateFileStat((const OOutlivableRef<OFileStatImp>&)filestat, stat);
}

error_t ODirectoryImp::Rename(const char * path)
{
    return kErrorNotImplemented;
}

error_t ODirectoryImp::Lock()
{
    return kErrorNotImplemented;
}

error_t ODirectoryImp::Unlock()
{
    return kErrorNotImplemented;
}

error_t ODirectoryImp::LockState(bool & state)
{
    state = _locked;
    return kStatusOkay;
}

error_t   OpenDirectory(const OOutlivableRef<ODirectory> & dir, vfsmount_k mnt, dentry_k entry)
{
    file_k filp;

    if (!(entry))
        return kErrorIllegalBadArgument;

    if (!(mnt))
        return kErrorIllegalBadArgument;

    if (!(dir.PassOwnership(new ODirectoryImp(mnt, entry))))
        return kErrorOutOfMemory;
    return kStatusOkay;
}

error_t   OpenDirectory(const OOutlivableRef<ODirectory> & dir, const char * path)
{
    error_t err;
    file_k filp;

    err = kStatusOkay;

    if (!(path))
        return kErrorIllegalBadArgument;

    filp = filp_open(path, kFileDirOnly, 0777);
    if (LINUX_PTR_ERROR(filp))
        return kErrorInternalError;

    if (!(dir.PassOwnership(new ODirectoryImp(IFile(filp).GetPath()))))
        err = kErrorOutOfMemory;

    filp_close(filp, 0);
    return err;
}

error_t CreateDirectory(const OOutlivableRef<ODirectory> & dir, const char * path, umode_t mode)
{
    error_t ret;
    dentry_k entry;
    path_k temp_path;
    inode_k inode;

    if (!(path))
        return kErrorIllegalBadArgument;
    
    ret          = kStatusOkay;
    temp_path    = alloca(path_size());

    entry        = kern_path_create(AT_FDCWD, path, temp_path, LOOKUP_DIRECTORY);
    inode        = (inode_k)IDEntry(IPath(temp_path).GetDEntry()).GetVarINode().GetUInt();

    if (vfs_mkdir(inode, entry, mode) != 0)
        ret = XENUS_ERROR_INTERNAL_ERROR;

    done_path_create(temp_path, entry); //some cleanup is required, some ref counters need decrementing (ie path_put), etc

    if (ERROR(ret))
        return ret;

    return OpenDirectory(dir, path);
}
