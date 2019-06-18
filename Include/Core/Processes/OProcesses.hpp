/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
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
};

typedef void(*ThreadIterator_cb)(OPtr<OProcessThread> thread, void * context);

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
                                                                                                
    virtual error_t UpdateThreadCache()                                                         = 0; // WARNING: 
    virtual uint_t GetThreadCount()                                                             = 0; // On Linux, these may lock execution if the RCU locking mechanism has been called prior 
    virtual error_t IterateThreads(ThreadIterator_cb callback, void * ctx)                      = 0; // Assume RCU locked if within callback (IE: GetProcessesByAll), assume unlocked otherwise
                                                                                                
    virtual error_t Terminate(bool force)                                                       = 0;

    virtual error_t ReadProcessMemory(user_addr_t address, void * buffer, size_t length)        = 0;
    virtual error_t WriteProcessMemory(user_addr_t address, const void * buffer, size_t length) = 0;

    virtual bool    Is32Bits()                                                                  = 0;
};

typedef void(*ProcessIterator_cb)(OPtr<OProcess> thread, void * context);

LIBLINUX_SYM  error_t GetProcessById(uint_t id, const OOutlivableRef<OProcess> process);
LIBLINUX_SYM  error_t GetProcessByCurrent(const OOutlivableRef<OProcess> process);

LIBLINUX_SYM error_t GetProcessesByAll(ProcessIterator_cb callback, void * data);

typedef void(*ProcessStartNtfy_cb)(OPtr<OProcess> thread); 
typedef void(*ProcessExitNtfy_cb)(OPtr<OProcess> thread); 

LIBLINUX_SYM error_t ProcessesAddExitHook(ProcessExitNtfy_cb cb);
LIBLINUX_SYM error_t ProcessesAddStartHook(ProcessStartNtfy_cb cb);

LIBLINUX_SYM error_t ProcessesRemoveExitHook(ProcessExitNtfy_cb cb);
LIBLINUX_SYM error_t ProcessesRemoveStartHook(ProcessStartNtfy_cb cb);
