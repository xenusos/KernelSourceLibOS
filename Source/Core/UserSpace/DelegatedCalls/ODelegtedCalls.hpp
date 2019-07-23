/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#include <Core\UserSpace\ODelegatedCalls.hpp>

const uint8_t BUILTIN_CALL_DB_PULL  = 3;
const uint8_t BUILTIN_CALL_EXTENDED = 4;
const uint8_t BUTLTIN_CALL_NTFY_COMPLETE = 5;
const uint8_t BUILTIN_CALL_SHORT = 6;

extern error_t AddKernelSymbol(const char * name, DelegatedCall_t fn);
extern void DelegatedCallsSysCallHandler(xenus_syscall_ref atten);

extern void InitDelegatedCalls();
