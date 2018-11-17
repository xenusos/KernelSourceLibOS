#pragma once

#include <Core/Processes/OProcesses.hpp>

void HackProcessesOThreadHook();
void HackProcessesAppendTskExitListener();
void InitProcesses();
void InitProcessTracking();


void ProcessesMMLock(mm_struct_k mm);
void ProcessesMMUnlock(mm_struct_k mm);
void ProcessesLockTask(task_k tsk);
void ProcessesUnlockTask(task_k tsk);
void ProcessesTaskStructInc(task_k  tsk);
void ProcessesTaskStructDec(task_k  tsk);


#define GENERIC_PATH 256
#define GENERIC_NAME 100
#define THREAD_NAME_LEN  GENERIC_NAME
#define PROCESS_NAME_LEN GENERIC_NAME

class OProcessThreadImpl : public  OProcessThread
{
public:
    OProcessThreadImpl(task_k tsk, OProcess * process);

    error_t GetThreadName(const char ** name) override;
    error_t GetOSHandle(void ** handle) override;
    error_t GetId(uint_t * id) override;
    error_t GetParent(const OUncontrollableRef<OProcess> parent) override;
protected:
    void InvaildateImp();
private:
    char _name[GENERIC_NAME];
    task_k _tsk;
    uint_t _id;
    OProcess * _process;
};


class OProcessImpl : public  OProcess
{
public:
    OProcessImpl(task_k tsk);

    void InitModName();
    void InitPaths();
    void InitSec();

    error_t GetProcessName(const char ** name) override;
    error_t GetOSHandle(void ** handle) override;
    error_t GetProcessId(uint_t * id) override;
    error_t GetModulePath(const char **path) override;
    error_t GetDrive(const char **mnt) override;
    error_t GetWorkingDirectory(const char **wd) override;
    error_t GetGenericSecLevel(ProcessSecurityLevel_e * sec) override;

    error_t UpdateThreadCache() override;
    uint_t GetThreadCount() override;
    error_t IterateThreads(ThreadIterator_cb callback, void * ctx) override;

    error_t Terminate(bool force) override;


    error_t AccessProcessMemory(user_addr_t address, void * buffer, size_t length, bool read);
    error_t ReadProcessMemory(user_addr_t address, void * buffer, size_t length)  override;
    error_t WriteProcessMemory(user_addr_t address, const void * buffer, size_t length) override;
    
protected:
    void InvaildateImp();

private:
    char _name[GENERIC_NAME];
    char _path[GENERIC_PATH];
    char _root[GENERIC_PATH];
    char _working[GENERIC_PATH];
    mutex_k _threads_mutex;
    dyn_list_head_p _threads;
    uint_t _pid;
    ProcessSecurityLevel_e _lvl;
    task_k _tsk;
    OProcessThreadImpl * _main_thread;
};
