/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson
*/  
#pragma once

#define LIB_OS_PRESENT

#if defined(LIBLINUX_BUILDING)
    #define LIBLINUX_SYM __declspec(dllexport) 
    #define LIBLINUX_ASM
#else 
    #define LIBLINUX_SYM __declspec(dllimport) 
    #define LIBLINUX_ASM __declspec(dllimport) 
#endif

#if defined(LIBLINUX_BUILDING) && !defined(LIB_COMPILER_PRESENT)
    #include <xenus_lazy.h>
    #include <libtypes.hpp>
    #include <libcompiler.hpp>
#endif

#include "Core\Base\Objects.hpp"

enum LoggingLevel_e
{
    kLogInfo,
    kLogWarning,
    kLogError,
    kLogDbg,

    KInvalidLogLevel
};

LIBLINUX_SYM void LoggingPrint(const char * mod, LoggingLevel_e lvl, const char * msg, va_list list);

// include core/logging and use LogPrint within your source files!
static inline void LogPrintEx(const char * mod, LoggingLevel_e lvl, const char * msg, ...)
{
    va_list list;

    va_start(list, msg);
    LoggingPrint(mod, lvl, msg, list);
    va_end(list);
}