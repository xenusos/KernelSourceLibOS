/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#pragma once

#include <ITypes/IThreadStruct.hpp>
#include <ITypes/ITask.hpp>

namespace CPU
{
    namespace Current
    {
        class LIBLINUX_CLS LinuxCurrent
        {
        public:
            LinuxCurrent() : _task_i(OSThread), _task(OSThread), _addr_pushed(false), _addr_limit(0)
            {}

            void SetCPUAffinity(cpumask mask);
            void GetCPUAffinity(cpumask & mask);

            void PushAddressLimit();
            void PopAddressLimit();

            void SnoozeMS(uint32_t ms);
            void SnoozeNanoRange(uint64_t u, uint64_t mu);

            l_int GetRealPRIO();    /*  prio  */
            l_int GetPrio();        /* static */

            l_uint GetCPU();

            void StopPreemption();
            void StartPreemption();

            int32_t GetNice();
            void SetNice(int32_t ahh);
        private:
            uint32_t _addr_limit;
            task_k _task;
            bool _addr_pushed;
#pragma warning(push)
#pragma warning(disable: 4251)
            ITask _task_i;
#pragma warning(pop)
        };
    }
}
