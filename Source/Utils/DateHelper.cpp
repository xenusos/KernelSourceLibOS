/*
    Purpose: 
    Author: Reece W. 
    License: All Rights Reserved J. Reece Wilson (2018-01-12)
    License: Released under MIT License (https://en.wikipedia.org/wiki/MIT_License) (2018-06-26)
*/  
#include <libos.hpp>
#include "DateHelper.hpp"

static const char * month_names[] = {
    "January", "Febuary", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"
};

static const char * day_names[] = {
    "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"
};

static timezone * sys_tz = nullptr;
static int64_t tz_offset = -1;

static bool TimeIsLeapYear(int year); 
static void TimeGetYear(uint64_t ms, int * years_passed, int * days_passed);
static void TimeMonthCalc(int year, int day, int * out_month, int * out_days);

int64_t DateHelpers::GetTimeZoneOffset()
{
    if (!sys_tz)
        sys_tz = (timezone *)kallsyms_lookup_name("sys_tz");

    if (!sys_tz)
        return 0;

    if (tz_offset != -1)
        return tz_offset;

    return tz_offset = -sys_tz->tz_minuteswest * 60000;
}

uint64_t DateHelpers::GetUnixTime()
{
    return NS_TO_MS(ktime_get_with_offset(TK_OFFS_REAL));
}

uint64_t DateHelpers::GetMonoTime()
{
    return ktime_get_with_offset(TK_OFFS_REAL);
}

uint64_t DateHelpers::GetBootTime()
{
    return ktime_get_with_offset(TK_OFFS_BOOT);
}

void DateHelpers::ParseTime(uint64_t ms, time_info & timeinfo)
{
    int year;
    int month;
    int days_start_month;
    int rem_days;
    int days_start_year;

    TimeGetYear(ms, &year, &days_start_year);

    rem_days = (MAX(1, ms / 86400000) - days_start_year);

    TimeMonthCalc(year, rem_days, &month, &days_start_month);

    timeinfo.year               = year;

    timeinfo.month.index        = month;
    timeinfo.month.month        = month + 1;
    timeinfo.month.name         = month_names[month];
    
    timeinfo.day.day_of_year    = rem_days;
    timeinfo.day.day_of_week    = (MAX(1, (ms % 604800000)) / 86400000) - 3; //epoch starts on thursday
    timeinfo.day.day_of_month   = (rem_days - days_start_month) + 1;
    timeinfo.day.index          = timeinfo.day.day_of_week - 1;
    timeinfo.day.name           = day_names[timeinfo.day.index];

    timeinfo.time.hours         = (MAX(1, (ms % 86400000)) / 3600000);
    timeinfo.time.minutes       = (MAX(1, (ms % 3600000)) / 60000);
    timeinfo.time.seconds       = (MAX(1, (ms % 60000)) / 1000);
    timeinfo.time.milliseconds  = (ms % 1000);
}

static size_t FormatSharedISO8601(char * str, size_t length, time_info & time, int64_t tz, bool illegalms);

size_t DateHelpers::FormatISO8601(char * str, size_t length, time_info & time, int64_t tz)
{
    return FormatSharedISO8601(str, length, time, tz, false);
}

size_t DateHelpers::FormatNonStd(char * str, size_t length, time_info & timeinfo, int64_t tz)
{
    return FormatSharedISO8601(str, length, timeinfo, tz, true);
}

size_t FormatSharedISO8601(char * str, size_t length, time_info & time, int64_t tz, bool illegalms)
{
    char timezone[sizeof("+??:??.")];

    printf("timezone: %zx\n", tz);

    if (tz != 0)
    {
        bool neg = tz < 0;
        uint32_t hrs, mins;

        hrs  = (MAX(1, (tz % 86400000)) / 3600000);
        mins = (MAX(1, (tz % 3600000)) / 60000);

        snprintf(timezone, 8, "%c%02d:%02d", neg ? '-' : '+', hrs, mins);
    }

    if (!illegalms)
    {
        return snprintf(str, length,
            "%04d-%02d-%02dT%02d:%02d:%02d.%03d%s",
            time.year,
            time.month.month,
            time.day.day_of_month,

            time.time.hours,
            time.time.minutes,
            time.time.seconds,
            time.time.milliseconds,

            tz == 0 ? "Z" : timezone);
    }
    else
    {
        return snprintf(str, length,
            "%02d-%02d %02d:%02d:%02d.%04d%s",
            time.month.month,
            time.day.day_of_month,

            time.time.hours,
            time.time.minutes,
            time.time.seconds,
            time.time.milliseconds,

            tz == 0 ? "" : timezone);
    }
}

size_t DateHelpers::FormatISO8601(char * str, size_t length, uint64_t ms)
{
    time_info time;
    DateHelpers::ParseTime(ms, time);
    return FormatISO8601(str, length, time);
}

static bool TimeIsLeapYear(int year)
{
    return ((year % 400 == 0 || year % 100 != 0) && (year % 4 == 0));
}

static void TimeGetDaysInYearMonth(int * months, int year)
{
    int month_cnt[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };

    if (TimeIsLeapYear(year))
        month_cnt[1] = 29;

    memcpy(months, month_cnt, sizeof(month_cnt));
}

static void TimeGetYear(uint64_t ms, int * o_years_passed, int * o_days_passed)
{
    int i;
    int32_t fuck;
    int32_t you;
    int32_t year;

    fuck    = int32_t(ms / uint64_t(86400000));
    you     = 0;
    year    = 1970;
    while (fuck - (i = TimeIsLeapYear(year) ? 366 : 365) >= 0)
    {
        fuck -= i;
        you  += i;
        year ++;
    }
    *o_years_passed    = year;
    *o_days_passed    = you;
}

static void TimeMonthCalc(int year, int day, int * out_month, int * out_days)
{
    int months[12];
    int days = 0;
    int month = 0;

    TimeGetDaysInYearMonth((int *)&months, year);

    while (days + months[month] <= day)
        days += months[month++];

    *out_days     = days;
    *out_month    = month;
}
