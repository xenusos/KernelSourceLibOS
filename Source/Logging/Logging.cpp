/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#include <libos.hpp>
#include <Core\FIO\OFile.hpp>
#include <Core\FIO\ODirectory.hpp>
#include <Utils\DateHelper.hpp>

static mutex_k logging_mutex;
static char logging_tline[PRINTF_MAX_STRING_LENGTH];
static char logging_tstr[PRINTF_MAX_STRING_LENGTH];
static OFile * log_file;

static const char * logging_levels[KInvalidLogLevel] = 
{
    "INFO",
    "VERBOSE",
    "WARNING",
    "ERROR",
    "DEBUG"
};

#define LOG_DIR "/Xenus/Kernel/Logs"

#define TS_LENGTH sizeof("????-??-??T??:??:??.???T??-?? PADDING")
#define FN_LEN    (sizeof(LOG_DIR "/Log ??.txt") + TS_LENGTH)

static void LoggingGetTs(char * ts)
{
    time_info now;
    DateHelpers::ParseTime(GET_LOCAL_TIME, now);
    DateHelpers::FormatNonStd(ts, TS_LENGTH, now, DateHelpers::GetTimeZoneOffset());
}

static void LoggingAppendLine(const char * ln)
{
    mutex_lock(logging_mutex);
    if (log_file)
    {
        log_file->Write(ln);
        log_file->Write("\n");
    }
    printf("\r-------------------\r%s\n", ln);
    mutex_unlock(logging_mutex);
}

static const char * LoggingLevelStringify(LoggingLevel_e lvl)
{
    if (lvl >= KInvalidLogLevel)
        return "-_-_-_-";
    return logging_levels[lvl];
}

void LoggingPrint(const char * mod, LoggingLevel_e lvl, const char * msg, va_list list)
{
    char timestamp[TS_LENGTH];

    if (!msg)
        msg = "[NULL]";

    LoggingGetTs(timestamp);

    vsnprintf(logging_tstr, PRINTF_MAX_STRING_LENGTH, msg, list);
    snprintf(logging_tline, PRINTF_MAX_STRING_LENGTH, "%s [%-8s] <%s> %s", timestamp, LoggingLevelStringify(lvl), mod, logging_tstr);

    LoggingAppendLine(logging_tline);
}

static void LoggingInitAllocations()
{
    log_file = nullptr;

    logging_mutex = mutex_create();
    ASSERT(logging_mutex, "failed to create logging mutex");
}

static bool LoggingInitTryCreateDir(const OOutlivableRef<ODirectory> & dir)
{
    error_t err;
    
    err = OpenDirectory(dir, LOG_DIR);

    if (ERROR(err))
    {
        err = CreateDirectory(dir, LOG_DIR);
        if (ERROR(err))
        {
            printf("Couldn't create directory for Xenus Kernel Logging. Error: " PRINTF_ERROR, err);
            return false;
        }
    }

    return true;
}

static void LoggingInitResetDir(ODumbPointer<ODirectory> dir)
{
    error_t err;
    linked_list_head_p list;

    list = linked_list_create();
    ASSERT(list, "out of memory");

    dir->Iterate([](ODirectory * dir, const char * path, void * ctx)
    {
        linked_list_head_p list = (linked_list_head_p)ctx;
        linked_list_entry_p entry = linked_list_append(list, 256);
        if (entry)
        {
            memset(entry->data, 0, 256);
            memcpy(entry->data, path, strnlen(path, 255));
        }
    }, list);

    if (list->length < 20)
    {
        linked_list_destroy(list);
        return;
    }

    for (linked_list_entry_p cur = list->bottom; cur != NULL; cur = cur->next)
    {
        ODumbPointer<OFile> file;
        const char * path;
        char full[256];

        path = (const char *)cur->data;

        if (path[0] == '.')
            continue;

        full[0] = 0;
        strlcat(full, LOG_DIR, 256);
        strlcat(full, "/", 256);
        strlcat(full, path, 256);

        err = OpenFile(OOutlivableRef<OFile>(file), full, kFileReadOnly, 0700);
        if (ERROR(err))
            continue;

        file->Delete();
    }

    linked_list_destroy(list);
}

static void LoggingInitCreateFile()
{
    error_t err;
    char filename[FN_LEN];
    char timestamp[TS_LENGTH];

    LoggingGetTs(timestamp);
    snprintf(filename, FN_LEN, LOG_DIR "/Log %s.txt", timestamp);

    err = OpenFile(OOutlivableRef<OFile>(log_file), filename, kFileAppend | kFileReadWrite | kFileCreate, 0700);
    if (ERROR(err))
        printf("Couldn't create xenus log file %s " PRINTF_ERROR "\n", filename, err);
}

void LoggingInit()
{
    ODumbPointer<ODirectory> dir;

    LoggingInitAllocations();

    if (!LoggingInitTryCreateDir(OOutlivableRef<ODirectory>(dir)))
        return;

    LoggingInitResetDir(dir);
    LoggingInitCreateFile();
}
