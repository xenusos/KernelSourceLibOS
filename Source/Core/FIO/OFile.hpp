/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson
*/  
#pragma once
#include <Core\FIO\OFile.hpp>

class OLinuxFileImp : public OFile
{
public:
    OLinuxFileImp(const char * file, uint64_t flags, umode_t mode);

    error_t SetPos(loff_t offset, loff_t & o_offset, loff_t maxsize)                override;
    error_t Write(const void * buffer, size_t length, loff_t offset = 0)            override;
    error_t Write(const char * string, loff_t offset = 0)                           override;
    error_t Read(void * buffer, size_t length, size_t & read, loff_t offset = 0)    override;
    error_t Delete()                                                                override;
    error_t GoUp(const OOutlivableRef<ODirectory> & dir)                            override;
    error_t GetPath(const OOutlivableRef<OPath> & path)                             override;
    error_t Stat(const OOutlivableRef<OFileStat> & stat)                            override;
    error_t Rename(const char * path)                                               override;
    error_t LockWrite()                                                             override;
    error_t UnlockWrite()                                                           override;
    error_t LockAll()                                                               override;
    error_t UnlockAll()                                                             override;
    error_t Error()                                                                 override;
    uint64_t KernelError()                                                          override;
    void * /*file_k*/ KernelHandle()                                                override;

protected:
    void InvalidateImp()                                                            override;

private:
    error_t  _error = kStatusOkay;
    uint64_t _flags;
    char     _path[256] = { 0 };
    file_k   _filp = nullptr;
    IFile    _fili;
    umode_t  _mode;
    uint64_t _linux_error;
};
