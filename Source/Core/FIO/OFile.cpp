/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson
*/  
#include <libos.hpp>

#include <ITypes\IPath.hpp>
#include <ITypes\IFile.hpp>
#include <ITypes\IKstat.hpp>
#include <ITypes\IDEntry.hpp>

#include "OPath.hpp"
#include "OFile.hpp"
#include "ODirectory.hpp"
#include "OFileStat.hpp"

#include <Utils\FileIOHelper.hpp>

OLinuxFileImp::OLinuxFileImp(const char * file, uint64_t flags, umode_t mode)
{
    memcpy(_path, file, strlen(file) + 1);

    _flags = flags;
    _mode  = mode;

    _filp  = filp_open(file, _flags, mode);
    if (!(_filp))
    {
        _filp  = nullptr;
        _error = kErrorFileNullHandle;
        return;
    }

    if (LINUX_PTR_ERROR(_filp))
    {
        _linux_error = (uint64_t)_filp;
        _filp        = nullptr;
        _error       = kErrorFileNullHandle;
    }

    _fili = IFile(_filp);
}

void OLinuxFileImp::InvalidateImp()
{
    if (_filp)
    {
        filp_close(_filp, 0);
        _filp = nullptr;
    }
    _error = kErrorFileNullHandle;
}

error_t OLinuxFileImp::Write(const void * buffer, size_t length, loff_t offset)
{
    size_t of;

    if (ERROR(_error)) 
        return _error;
    
    of = offset;

    if (kernel_write(_filp, buffer, length, &offset) != length)
        return kErrorGenericFailure;

    return kStatusOkay;
}

error_t OLinuxFileImp::Write(const char * string, loff_t offset)
{
    return Write((const void *)string, strlen(string), 0);
}

error_t OLinuxFileImp::Read(void * buffer, size_t length, size_t & read, loff_t offset)
{
    loff_t pos;
    size_t len;

    if (ERROR(_error))
        return _error;

    pos = offset;

    if ((len = kernel_read(_filp, buffer, length, &pos)) != length)
    {
        read = len;
        return kStatusBufferNotFilled;
    }

    return kStatusOkay;
}

error_t OLinuxFileImp::Delete()
{
    error_t err;
    ODumbPointer<OLinuxPathImpl> file_path;
    ODumbPointer<OLinuxPathImpl> dir_path;

    if (ERROR(err = _error))
        return err;

    if (ERROR(err = OpenLinuxPath(OOutlivableRef<OLinuxPathImpl>(file_path), _fili.GetPath())))
        return err;

    if (ERROR(err = file_path->GetParent_1(dir_path)))
        return err;

    vfs_unlink(dir_path->ToINode(), file_path->GetDEntry(), nullptr);
    InvalidateImp();
    return err;
}

error_t OLinuxFileImp::SetPos(loff_t offset, loff_t & o_offset, loff_t maxsize)
{
    return kErrorNotImplemented;
}

error_t OLinuxFileImp::Rename(const char * path)
{
    return kErrorNotImplemented;
}

error_t OLinuxFileImp::LockWrite()
{
    return kErrorNotImplemented;
}

error_t OLinuxFileImp::UnlockWrite()
{
    return kErrorNotImplemented;
}

error_t OLinuxFileImp::LockAll()
{
    return kErrorNotImplemented;
}

error_t OLinuxFileImp::UnlockAll()
{
    return kErrorNotImplemented;
}

error_t OLinuxFileImp::GoUp(const OOutlivableRef<ODirectory> & dir)
{
    char dir_path[256];
    
    if (ERROR(_error))
        return _error;

    if (!IOHelpers::UpDir(_path, dir_path))
        return kErrorGenericFailure;
    
    //TOOD: obain  dentry, use that to construct ODirectoryImp
    return OpenDirectory(dir, dir_path);
}

error_t OLinuxFileImp::GetPath(const OOutlivableRef<OPath> & path)
{
    if (ERROR(_error))
        return _error;

    return OpenLinuxPath((OOutlivableRef<OLinuxPathImpl>&)path, _fili.GetPath());
}

error_t OLinuxFileImp::Stat(const OOutlivableRef<OFileStat> & filestat)
{
    kstat_k stat;

    stat = alloca(kstat_size());

    if (vfs_getattr(_fili.GetPath(), stat, STATX_ALL, AT_STATX_SYNC_AS_STAT) != 0)
        return kErrorInternalError;

    return CreateFileStat((const OOutlivableRef<OFileStatImp>&)filestat, stat);
}

error_t OLinuxFileImp::Error()
{
    return _error;
}

void * OLinuxFileImp::KernelHandle()
{
    return (void *)_filp;
}

uint64_t OLinuxFileImp::KernelError()
{
    return _linux_error;
}

error_t OpenFile(const OOutlivableRef<OFile>& ofile, const char * file, uint64_t flags, umode_t mode)
{
    if (!file)
        return kErrorIllegalBadArgument;

    if (!(ofile.PassOwnership(new OLinuxFileImp(file, flags, mode))))
        return kErrorOutOfMemory;

    return kStatusOkay;
}
