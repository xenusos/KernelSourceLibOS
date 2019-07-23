/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#define THREAD_ENABLE_CLEANUP_ACCESS
#include <libos.hpp>
#include "ORegistration.hpp"

#include "DelegatedCalls/ODelegtedCalls.hpp"
#include "Files/OPseudoFile.hpp"
#include "../Processes/OProcessTracking.hpp"

static OPtr<OPseudoFile> registration_file;

static void RegisterCurrent()
{
    threading_set_process_syscall_handler(DelegatedCallsSysCallHandler);
    thread_enable_cleanup();
    ProcessesTryRegisterLeader(OSThread);
}

void InitRegistration()
{
    error_t er;
    const void * priv;
    
    size_t we_lied; 

    er = CreateTempKernFile(OOutlivableRef<OPseudoFile>(registration_file));
    ASSERT(NO_ERROR(er), "delegated calls couldn't create file %lli", er);

    registration_file->GetIdentifierBlob(&priv, we_lied);

    const PsudoFileInformation_p info = (const PsudoFileInformation_p)priv;

    ASSERT(info->pub.devfs.char_dev_id == 0, "Registration file couldn't be registered; someone beat us to id zero");
    registration_file->OnOpen([](OPtr<OPseudoFile> file, void ** context)
    {
        RegisterCurrent();
        *context = nullptr;
        return true;
    });
}
