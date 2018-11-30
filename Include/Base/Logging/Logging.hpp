/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
    Note: REINCLUDABLE
*/

#if !defined(LOGGING_DECLARED)
#define LOGGING_DECLARED

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

#endif // #if defined(LOGGING_DECLARED)

#if defined(LogPrint)
    #undef LogPrint
#endif

#if defined(LOG_MOD_)
    #undef LOG_MOD_
#endif

#if defined(LOG_NO_IDNT)
    #undef LOG_NO_IDNT
#endif

#if !defined(LOG_MOD)
    #define LOG_NO_IDNT
    #define LOG_MOD_ "PreprocessorError"
#else
    #define LOG_MOD_ LOG_MOD
#endif 

#define LogPrint(lvl, msg, ...) LogPrintEx(LOG_MOD_, lvl, msg, __VA_ARGS__);
