/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#include <libos.hpp>
#include <Utils/DateHelper.hpp>
#include <ITypes/IThreadStruct.hpp>
#include <ITypes/ITask.hpp>
#include "LinuxSleeping.hpp"

struct SleepState
{
    bool timeoutable;
    int64_t timeout;
};

static struct SleepState GetSleepState(uint32_t ms)
{
    SleepState state = { 0 };

    if (ms == -1)
        return state;

    state.timeout     = MAX(1, MSToOSTicks(ms));
    state.timeoutable = true;

    return state;
}

static inline bool LinuxSleepTimedout(struct SleepState * state)
{
    return state->timeoutable && state->timeout == 0;
}

bool LinuxSleep(uint32_t ms, bool(*callback)(void * context), void * context)
{
    ITask tsk(OSThread);
    bool signaled = false;
    uint_t ustate = 0;
    struct SleepState state;

    state  = GetSleepState(ms);
    ustate = tsk.GetVarState().GetUInt();

    while (true)
    {
        if (callback(context))
        {
            signaled = true;
            break;
        }

        if (LinuxSleepTimedout(&state))
            break;

        if (state.timeoutable)
        {
            state.timeout = schedule_timeout_interruptible(state.timeout);
            continue;
        }

        tsk.GetVarState().Set((uint_t)TASK_INTERRUPTIBLE);
        schedule();
    }

    tsk.GetVarState().Set(ustate);
    
    return signaled;
}

void LinuxPokeThread(task_k task)
{
    wake_up_process(task);
}
