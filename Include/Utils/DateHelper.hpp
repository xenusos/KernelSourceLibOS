/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson
*/  
#pragma once

#define NS_TO_MS(NS) ((NS) / 1000000)
#define MS_TO_NS(MS) ((MS) * 1000000)
#define MS_TO_S(MS)  ((MS) / 1000)
#define NS_TO_S(NS)  ((NS) / 1000000000)
#define S_TO_MS(S)   ((S) * 1000)
#define S_TO_NS(S)   ((S) * 1000000000)

#define TIMESPEC_PTR_TO_MS(ptr)    (S_TO_MS(ptr->tv_sec) + NS_TO_MS(ptr->tv_nsec))
#define TIMESPEC_TO_MS(ptr)        (S_TO_MS(ptr.tv_sec)  + NS_TO_MS(ptr.tv_nsec))

#define GET_TZ_MS        (DateHelpers::GetTimeZoneOffset())
#define MS_APPEND_TZ(n)    ((n) + GET_TZ_MS)

#define GET_TIME_EPOCH  DateHelpers::GetUnixTime()
#define GET_LOCAL_TIME    (MS_APPEND_TZ(GET_TIME_EPOCH))

static inline uint32_t MSToOSTicks(uint64_t ms)
{
    uint64_t HZ = kernel_information.KERNEL_FREQUENCY;
    if (ms > 1000)
        return uint32_t(HZ * ms / 1000);
    else
        return uint32_t(HZ / (1000 / ms));
}

struct time_info
{
    int year;
    
    struct
    {
        int index;
        int month;
        const char * name;
    } month;

    struct
    {
        const char * name;
        int index;
        int day_of_week;
        int day_of_month;
        int day_of_year;
    } day;

    struct
    {
        int hours;
        int minutes;
        int seconds;
        int milliseconds;
    } time;
};

namespace DateHelpers
{
    LIBLINUX_SYM uint64_t GetUnixTime();
    LIBLINUX_SYM int64_t  GetTimeZoneOffset();
    LIBLINUX_SYM void     ParseTime(uint64_t ms, time_info & timeinfo);
    LIBLINUX_SYM size_t   FormatISO8601(char * str, size_t length, uint64_t ms);
    LIBLINUX_SYM size_t   FormatISO8601(char * str, size_t length, time_info & timeinfo, int64_t tz = 0);
    LIBLINUX_SYM size_t   FormatNonStd(char * str, size_t length, time_info & timeinfo, int64_t tz = 0);
}
