/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson
*/
#include <libos.hpp>
#include <Core\FIO\OFile.hpp>
#include <Core\FIO\ODirectory.hpp>
#include <Utils\DateHelper.hpp>

static mutex_k logging_mutex;
static char * logging_tline;
static char * logging_tstr;
static OFile * log_file;

static const char * logging_levels[KInvalidLogLevel] = 
{
    "INFO",
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

    logging_tline = (char *)malloc(PRINTF_MAX_STRING_LENGTH);
    logging_tstr  = (char *)malloc(PRINTF_MAX_STRING_LENGTH);

    ASSERT(logging_tline, "couldn't allocate temp log line buffer");
    ASSERT(logging_tstr, "couldn't allocate temp log string buffer");
}

static bool LoggingInitTryCreateDir(const OOutlivableRef<ODirectory> & dir)
{
    error_t err;
    if (ERROR(err = OpenDirectory(dir, LOG_DIR)))
    {
        if (ERROR(err = CreateDirectory(dir, LOG_DIR)))
        {
            printf("COULDN'T CREATE LOG DIRECTORY - XENUS KERNEL");
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
            memcpy(entry->data, path, MIN(strlen(path), 255));
        }
    }, list);

    if (list->length < 20)
    {
        linked_list_destory(list);
        return;
    }

    for (linked_list_entry_p cur = list->bottom; cur != NULL; cur = cur->next)
    {
        ODumbPointer<OFile> file;
        const char * path;
        char full[256];
        size_t len;

        path = (const char *)cur->data;
        len = strlen(path);

        if (path[0] == '.')
            continue;

        memset(full, 0, 256);
        strlcat(full, LOG_DIR, 256);
        strlcat(full, "/", 256);
        strlcat(full, path, 256);

        if (ERROR(err = OpenFile(OOutlivableRef<OFile>(file), full, kFileReadOnly, 0777)))
            continue;

        file->Delete();
    }

    linked_list_destory(list);
}

static void LoggingInitCreateFile()
{
    error_t err;
    char filename[FN_LEN];
    char timestamp[TS_LENGTH];

    LoggingGetTs(timestamp);
    snprintf(filename, FN_LEN, LOG_DIR "/Log %s.txt", timestamp);

    if (ERROR(err = OpenFile(OOutlivableRef<OFile>(log_file), filename, kFileAppend | kFileReadWrite | kFileCreate, 0777)))
    {
        printf("Couldn't create xenus log file %s %lli \n", filename, err);
    }
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
