#pragma once

static inline void LogPrint(LoggingLevel_e lvl, const char * msg, ...)
{
    va_list list;

    va_start(list, msg);
    LoggingPrint(LOG_MOD, lvl, msg, list);
    va_end(list);
}