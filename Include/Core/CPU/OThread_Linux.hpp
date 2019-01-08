#pragma once

class ITask;
class LIBLINUX_CLS LinuxCurrent
{
public:
    LinuxCurrent();

    void SetCPUAffinity(cpumask mask);
    void GetCPUAffinity(cpumask & mask);

    void PushAddressLimit();
    void PopAddressLimit();

    void SnoozeMS(uint64_t ms);
    void SnoozeNanoRange(uint64_t u, uint64_t mu);

    l_int GetRealPRIO();    /*  prio  */
    l_int GetPrio();		/* static */

    l_uint GetCPU();

    void StopPreemption();
    void StartPreemption();

    int32_t GetNice();
    void SetNice(int32_t ahh);
private:
    uint32_t _addr_limit;
    task_k _task;
    bool _addr_pushed;
    ITask * _task_i;
};

