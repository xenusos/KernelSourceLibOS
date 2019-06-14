/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#include <Core\UserSpace\ODelegatedCalls.hpp>

#define BUILTIN_CALL_DB_PULL 3
#define BUILTIN_CALL_EXTENDED 4
#define BUTLTIN_CALL_NTFY_COMPLETE 5
#define BUILTIN_CALL_SHORT 6

extern error_t AddKernelSymbol(const char * name, DelegatedCall_t fn);
extern void DelegatedCallsSysCallHandler(xenus_syscall_ref atten);

extern void InitDelegatedCalls();
