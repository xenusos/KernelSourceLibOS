/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#pragma once

extern uint_t ProcessesGetTgid(task_k tsk);
extern uint_t ProcessesGetPid(task_k tsk);

extern task_k ProcessesGetGroupLeader(task_k task);
extern task_k ProcessesGetProcess(task_k task);

extern void ProcessesMMIncrementCounter(mm_struct_k mm);
extern void ProcessesMMDecrementCounter(mm_struct_k mm);
extern void ProcessesAcquireTaskFields(task_k tsk);
extern void ProcessesReleaseTaskFields(task_k tsk);
extern void ProcessesTaskIncrementCounter(task_k  tsk);
extern void ProcessesTaskDecrementCounter(task_k  tsk);

extern mm_struct_k ProcessesAcquireMM(task_k tsk);
extern mm_struct_k ProcessesAcquireMM_Read(task_k tsk);
extern mm_struct_k ProcessesAcquireMM_Write(task_k tsk);


extern void ProcessesAcquireMM_LockRead(mm_struct_k mm);
extern void ProcessesAcquireMM_LockWrite(mm_struct_k mm);
extern void ProcessesReleaseMM_UnlockRead(mm_struct_k mm);
extern void ProcessesReleaseMM_UnlockWrite(mm_struct_k mm);

#define ProcessesReleaseMM_NoLock ProcessesMMDecrementCounter
extern void ProcessesReleaseMM_Read(mm_struct_k mm);
extern void ProcessesReleaseMM_Write(mm_struct_k mm);
