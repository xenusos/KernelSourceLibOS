/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#pragma once
class ODEWorkHandler;
class ODEImplProcess;
class OLMemoryAllocation;

struct DERestoreThread
{
    bool hasPreviousTask;
    pt_regs restore;
};

struct DEStackMapping
{
    union
    {
        size_t address;
        size_t bottom;
        size_t start;
    };
    union
    {
        size_t top;
        size_t end;
        size_t sp;
    };
    OPtr<OLMemoryAllocation> allocation;
};

struct DEStack
{
    page_k * pages;
    DEStackMapping user;
    DEStackMapping kernel;
};

class ODEImplPIDThread
{
public:
    ODEImplPIDThread(ODEImplProcess * parent);
    ~ODEImplPIDThread();

    error_t           Init();

    void              UpdatePidHandle(task_k task);

    error_t           AppendWork(ODEWorkHandler * handler);
    void              NtfyJobFinished(size_t ret);
    DEStack *         GetStack();

protected:

    error_t AllocateStack();
    error_t AllocatePendingWork();

    void PopCompletedTask(ODEWorkHandler * & current, bool & hasNextJob, ODEWorkHandler * & nextJob);

    void PreemptExecution(pt_regs * registers, bool kick);
    void PreemptExecutionForWork(ODEWorkHandler * exec, bool kick);

    void SaveState(pt_regs_p state);

private:
    dyn_list_head_p  _workPending = nullptr;
    DERestoreThread  _userState   = {0};
    DEStack          _stack       = {0};
    task_k           _task        = nullptr;
    ODEImplProcess * _proc        = nullptr;
};

extern error_t GetDEThread(ODEImplPIDThread * & thread, task_k task);
extern error_t GetOrCreateDEThread(ODEImplPIDThread * & thread, task_k task);
