/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/  
#pragma once

namespace IO
{
    class ODirectory;
    class OPath;
    class OFileStat;

    const uint64_t    kFileReadOnly       = O_RDONLY;
    const uint64_t    kFileWriteOnly      = O_WRONLY;
    const uint64_t    kFileReadWrite      = O_RDWR;
    const uint64_t    kFileAppend         = O_APPEND | O_RDWR;
    const uint64_t    kFileDirOnly        = O_DIRECTORY;
    const uint64_t    kFileIgnoreLinks    = O_NOFOLLOW;
    const uint64_t    kFileCreate         = O_CREAT;
    
    class OFile : public OObject
    {
    public:
        virtual error_t SetPos(loff_t offset, loff_t & o_offset, loff_t maxsize)                = 0;
        virtual error_t Write(const void * buffer, size_t length, loff_t offset = 0)            = 0;
        virtual error_t Write(const char * string, loff_t offset = 0)                           = 0;
        virtual error_t Read(void * buffer, size_t length, size_t & read, loff_t offset = 0)    = 0;
        virtual error_t Delete()                                                                = 0;                                        
        virtual error_t GoUp(const OOutlivableRef<ODirectory> & dir)                            = 0;
        virtual error_t GetPath(const OOutlivableRef<OPath> & path)                             = 0;
        virtual error_t Stat(const OOutlivableRef<OFileStat> & stat)                            = 0;
        virtual error_t Rename(const char * path)                                               = 0;
        virtual error_t LockWrite()                                                             = 0;
        virtual error_t UnlockWrite()                                                           = 0;
        virtual error_t LockAll()                                                               = 0;
        virtual error_t UnlockAll()                                                             = 0;
        virtual error_t Error()                                                                 = 0;
        virtual uint64_t KernelError()                                                          = 0;
        virtual void * /*file_k*/ KernelHandle()                                                = 0;
    };
    
    LIBLINUX_SYM error_t OpenFile(const OOutlivableRef<OFile> & ofile, const char * file, uint64_t flags, umode_t mode = 0777);
}
