#pragma once

enum LoggingLevel_e
{
    kLogInfo,
    kLogWarning,
    kLogError,
    kLogDbg,

    KInvalidLogLevel
};

LIBLINUX_SYM void LoggingPrint(const char * mod, LoggingLevel_e lvl, const char * msg, va_list list);

static inline void LogPrint(LoggingLevel_e lvl, const char * msg, ...)
{
    va_list list;

    va_start(list, msg);
    LoggingPrint(LOG_MOD, lvl, msg, list);
    va_end(list);
}