/*
    Purpose: 
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#pragma once
#include <Core/Processes/OProcesses.hpp>

extern void InitProcesses();
extern void InitProcessTracking();

extern task_k g_init_task;

#define GENERIC_PATH 256
#define GENERIC_NAME 100
#define THREAD_NAME_LEN  GENERIC_NAME
#define PROCESS_NAME_LEN GENERIC_NAME


class OProcessImpl : public  OProcess
{
public:
    OProcessImpl(task_k tsk);

    void TryInitModName();
    void TryInitPaths();
    void InitModName();
    void InitPaths();
    void InitSec();

    error_t GetProcessName(const char ** name)                                                   override;
    error_t GetOSHandle(void ** handle)                                                          override;
    error_t GetProcessId(uint_t * id)                                                            override;
    error_t GetModulePath(const char **path)                                                     override;
    error_t GetDrive(const char **mnt)                                                           override;
    error_t GetWorkingDirectory(const char **wd)                                                 override;
    //error_t GetGenericSecLevel(ProcessSecurityLevel_e * sec)                                     override;
                                                                                                 
    uint_t  GetThreadCount()                                                                     override;
    error_t IterateThreads(ThreadIterator_cb callback, void * ctx)                               override;
    error_t GetThreadById(uint_t id, const OOutlivableRef<OProcessThread> & thread)              override;
                                                                                                 
    error_t Terminate(bool force)                                                                override;


    error_t AccessProcessMemory(user_addr_t address, void * buffer, size_t length, bool read);
    error_t ReadProcessMemory(user_addr_t address, void * buffer, size_t length)                 override;
    error_t WriteProcessMemory(user_addr_t address, const void * buffer, size_t length)          override;
    
    bool Is32Bits()                                                                              override;
protected:
    void InvalidateImp();

private:
    char    _name[GENERIC_NAME];
    char    _path[GENERIC_PATH];
    char    _root[GENERIC_PATH];
    char    _working[GENERIC_PATH];
    uint_t  _pid;
    task_k  _tsk;
    bool    _initPaths;
    bool    _initName;
    //ProcessSecurityLevel_e  _lvl;
};
