/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#pragma once

class OProcess;
class OProcessThread : public  OObject
{
public:
    virtual error_t GetThreadName(const char ** name)                                           = 0;
    virtual error_t GetOSHandle(void ** handle)                                                 = 0;
    virtual error_t GetId(uint_t * id)                                                          = 0;
    virtual error_t GetParent(const OUncontrollableRef<OProcess> parent)                        = 0;

    virtual error_t IsProcess(bool * out)                                                       = 0;
    virtual error_t AsProcess(const OOutlivableRef<OProcess> parent)                            = 0;
};

typedef bool(*ThreadIterator_cb)(const OPtr<OProcessThread> thread, void * context);

class OProcess : public  OObject
{
public:
    virtual error_t GetProcessName(const char ** name)                                          = 0;
    virtual error_t GetOSHandle(void ** handle)                                                 = 0;
    virtual error_t GetProcessId(uint_t * id)                                                   = 0;
    virtual error_t GetModulePath(const char **path)                                            = 0;
    virtual error_t GetDrive(const char **mnt)                                                  = 0;
    virtual error_t GetWorkingDirectory(const char **wd)                                        = 0;
    // virtual error_t GetGenericSecLevel(ProcessSecurityLevel_e * sec) = 0;
    // TODO: user api           

    virtual uint_t  GetThreadCount()                                                            = 0;
    virtual error_t IterateThreads(ThreadIterator_cb callback, void * ctx)                      = 0;
    virtual error_t GetThreadById(uint_t id, const OOutlivableRef<OProcessThread> & thread)     = 0;
                                                                                                
    virtual error_t Terminate(bool force)                                                       = 0;

    virtual error_t ReadProcessMemory(user_addr_t address, void * buffer, size_t length)        = 0;
    virtual error_t WriteProcessMemory(user_addr_t address, const void * buffer, size_t length) = 0;

    virtual bool    Is32Bits()                                                                  = 0;
};

typedef bool(*ProcessIterator_cb)(OPtr<OProcess> thread, void * context);

LIBLINUX_SYM  error_t GetProcessParentById(uint_t id, const OOutlivableRef<OProcess> process);
LIBLINUX_SYM  error_t GetProcessById(uint_t id, const OOutlivableRef<OProcess> process);
LIBLINUX_SYM  error_t GetProcessByCurrent(const OOutlivableRef<OProcess> process);

LIBLINUX_SYM  error_t GetProcessesByAll(ProcessIterator_cb callback, void * data);
LIBLINUX_SYM  error_t GetProcessesAtRootLevel(ProcessIterator_cb callback, void * data);
LIBLINUX_SYM  uint_t  GetProcessCurrentId();
LIBLINUX_SYM  uint_t  GetProcessCurrentTid();
