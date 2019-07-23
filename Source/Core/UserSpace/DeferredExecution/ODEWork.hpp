/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#pragma once
#include <Core/UserSpace/ODeferredExecution.hpp>

class ODEWorkJobImpl;
class ODEWorkHandler
{
public:
    ODEWorkHandler(task_k tsk, ODEWorkJobImpl * work);
    ~ODEWorkHandler();

    error_t SetWork(ODEWork & work);
    error_t Schedule();
    void DeattachWorkObject();
    void Hit(size_t response);
    void Die();

    void SetupRegisters(pt_regs & regs);
    void SetupStack(size_t * sp);

private:
    ODEWorkJobImpl * _parant = nullptr;
    ODEWork          _work = { 0 };
    task_k           _tsk = nullptr;

};

extern void DestoryWorkHandler(ODEWorkJobImpl * handler);
extern void InitDEWorkHandlers();
