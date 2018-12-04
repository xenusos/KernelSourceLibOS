/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#include <libos.hpp>
#include "ORegistration.hpp"

#include "ODeferredExecution.hpp"
#include "ODelegtedCalls.hpp"
#include "OPseudoFile.hpp"
#include "../Processes/OProcessTracking.hpp"

OPtr<OPseudoFile> registration_file;

void RegisterCurrent()
{
    threading_set_process_syscall_handler(DelegatedCallsSysCallHandler);

    ProcessesTryRegisterLeader(OSThread);
}

void InitRegistration()
{
    error_t er;
    PsudoFileInformation_p info;
    size_t we_lied; 

    er = CreateTempKernFile(OOutlivableRef<OPseudoFile>(registration_file));

    ASSERT(NO_ERROR(er), "delegated calls couldn't create file %lli", er);

    registration_file->GetIdentifierBlob((const void **)&info, we_lied);

    ASSERT(info->pub.devfs.char_dev_id == 0, "Registration file couldn't be registered; someone beat us to id zero");

    registration_file->OnOpen([](OPtr<OPseudoFile> file)
    {
        RegisterCurrent();
        return true;
    });
}