/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#pragma once

extern bool LinuxSleep(uint32_t ms, bool(*callback)(void * context), void * context);
extern void LinuxPokeThread(task_k task);
