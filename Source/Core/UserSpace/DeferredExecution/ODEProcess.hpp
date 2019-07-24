/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#pragma once

class ODEImplPIDThread;
class ODEImplProcess
{
public:
    ODEImplProcess(task_k task, chain_p pids);
    ~ODEImplProcess();

    size_t                GetReturnAddress();
    ODEImplPIDThread *    GetOrCreateThread(task_k task);
    error_t               GetThread(task_k task, ODEImplPIDThread * & thread);

protected:
    void                  MapReturnStub();

private:
    chain_p _pids         = nullptr;
    task_k _task          = nullptr;
    size_t _returnAddress = 0;
};

extern error_t GetDEProcess(ODEImplProcess * & out, task_k task);
extern void FreeDEProcess(task_k task);

extern void InitDEProcesses();
