/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
    Depends: OProcess.hpp
*/
#pragma once
#include <Core/Processes/OProcesses.hpp>


class OProcessThreadImpl : public OProcessThread
{
public:
    OProcessThreadImpl(task_k tsk, bool isProc, OProcess * process);

    error_t GetThreadName(const char ** name)                    override;
    error_t GetOSHandle(void ** handle)                          override;
    error_t GetId(uint_t * id)                                   override;
    error_t GetParent(const OUncontrollableRef<OProcess> parent) override;

    error_t IsProcess(bool * out)                                override;
    error_t AsProcess(const OOutlivableRef<OProcess> parent)     override;
protected:
    void InvalidateImp();

private:
    char       _name[GENERIC_NAME + 1];
    task_k     _tsk;
    uint_t     _id;
    OProcess * _process;
    bool       _isProc;
};
