// ======================================================================
// Copyright (C) 2015 Associated Universities, Inc. Washington DC, USA.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
// Correspondence concerning GBT software should be addressed as follows:
//  GBT Operations
//  National Radio Astronomy Observatory
//  P. O. Box 2
//  Green Bank, WV 24944-0002 USA

#include "matrix/Time.h"
#include <time.h>
#include <sys/time.h>
#include <sstream>
#include <iomanip>
#include <string>

#ifndef NANOSEC_PER_SEC
#define NANOSEC_PER_SEC (1000000000LL)
#define NANOSEC_PER_DAY (86400000000000LL)
#endif

extern "C" void  __Matrix__Time__(...)
{
}

namespace Time
{

    clockid_t default_clock = CLOCK_REALTIME; // The 'normal' ntp clock

    void set_default_clock(clockid_t clkid)
    {
        default_clock = clkid;
    }

    Time_t getUTC(clockid_t clk)
    {
        timespec ts;
        clock_gettime(clk, &ts);
        return (static_cast<Time_t>(ts.tv_sec * NANOSEC_PER_SEC + ts.tv_nsec));
    }

    Time_t timespec2Time(const timespec &ts)
    {
        return (((Time_t)ts.tv_sec) * NANOSEC_PER_SEC + ts.tv_nsec);
    }

    void  time2timespec(const Time_t t, timespec &ts_result)
    {
        Time_t secs;
        secs = t;
        ts_result.tv_sec = (time_t)  (t/NANOSEC_PER_SEC);
        ts_result.tv_nsec= (long int)(t%NANOSEC_PER_SEC);
    }

    Time_t timeval2Time(const timeval &ts)
    {
        return (((Time_t)ts.tv_sec) * NANOSEC_PER_SEC + ts.tv_usec * 1000);
    }

    void  time2timeval(const Time_t t, timeval &tv_result)
    {
        timespec ts;
        time2timespec(t, ts);
        tv_result.tv_sec = ts.tv_sec;
        tv_result.tv_usec= ts.tv_nsec/1000;
    }


#define MJD_1970_EPOCH (40587)

// Note: This routine can only calculate MJD's for dates after 1970/1/1
    int MJD(const Time_t t)
    {
        Time_t days_since_1970 = t/NANOSEC_PER_DAY;
        return MJD_1970_EPOCH + (int)days_since_1970;
    }

// Note: This routine can only calculate MJD's for dates after 1970/1/1
    double DMJD(const Time_t t)
    {
        int days = MJD(t);
        double nsec = static_cast<double>(t%86400000000000LL);
        return static_cast<double>(days) + nsec/86400000000000.0;
    }

// Note: This routine can only calculate MJD's for dates after 1970/1/1
    Time_t timeStamp2Time(uint32_t mjd, uint32_t msec)
    {
        Time_t t = ((Time_t)(mjd - MJD_1970_EPOCH)) * NANOSEC_PER_DAY;
        t += ((Time_t)(msec)) * 1000000LL;
        return (t);
    }

// Same restriction as above, truncates precision down to millisecond level
    void  time2TimeStamp(const Time_t t, uint32_t &mjd, uint32_t &msec)
    {
        mjd  = MJD(t);
        msec = (unsigned int)((t/1000000LL)%86400000LL);
    }

// Same restriction as above, truncates precision down to millisecond level
    void  time2TimeStamp(const Time_t t, uint32_t &mjd, double &msec)
    {
        mjd  = MJD(t);
        msec = static_cast<double>(t%86400000000000LL)*1E-6;
    }

//                           N/A  J   F   M   A   M   J   J   A   S   O   N   D
    static int month_lengths[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    bool
    calendarDate(const Time_t t,   int &year,   int &month, int &dayofmonth,
                 int &hour, int &minute, double &sec)
    {
        int dayofyr;
        bool is_leap=false;
        int   days     = (int)(t/(NANOSEC_PER_DAY));
        Time_t nsec     =       t%(NANOSEC_PER_DAY);
        year = 1970; // Ah, the good olde days ...

        // increment days by 1 since calendar is 1 based, not zero based.
        days++;

        while (days > 365)
        {
            // leap year rules:
            // leap year in years evenly divisible by 4,
            // no leap in years evenly divisible by 100,
            // leap in years also evenly divisible by 400
            if (year%400 == 0)
            {
                is_leap=true;
            }
            else if (year%100 == 0)
            {
                is_leap=false;
            }
            else if (year%4 == 0)
            {
                is_leap=true;
            }
            else
            {
                is_leap=false;
            }

            if (is_leap)
            {
                // printf("%d is a leap year\n", year);
                days -= 366;
            }
            else
            {
                // printf("%d is NOT leap year\n", year);
                days -= 365;
            }
            year++;
        }

        dayofyr = days;

        if (year%400 == 0)
        {
            is_leap=true;
        }
        else if (year%100 == 0)
        {
            is_leap=false;
        }
        else if (year%4 == 0)
        {
            is_leap=true;
        }
        else
        {
            is_leap=false;
        }

        for (month=1;  ; ++month)
        {
            int extra_day = (month==2 && is_leap) ? 1 : 0;

            if (dayofyr <= (month_lengths[month] + extra_day))
            {
                dayofmonth = dayofyr;
                break;
            }
            dayofyr -= (month_lengths[month] + extra_day);
        }

        hour   = (int)(nsec/(3600*NANOSEC_PER_SEC));
        nsec   = nsec%(3600*NANOSEC_PER_SEC);
        minute = (int)(nsec/(60*NANOSEC_PER_SEC));
        nsec   = nsec%(60*NANOSEC_PER_SEC);
        sec    = (double)nsec/(double)NANOSEC_PER_SEC;

        return 1;
    }

    Time_t date2Time(const int year, const int month, const int dayofmonth,
                     const int msec_since_midnight)
    {
        struct tm ymdhms;
        ymdhms.tm_year = year - 1900;
        ymdhms.tm_mon = month - 1;
        ymdhms.tm_mday = dayofmonth;
        ymdhms.tm_hour = (msec_since_midnight / 3600000) % 24;
        ymdhms.tm_min = (msec_since_midnight / 60000) % 60;
        ymdhms.tm_sec = (msec_since_midnight / 1000) % 60;
        int msec = msec_since_midnight % 1000;
        time_t t = timegm(&ymdhms);
        return (Time_t)t * NANOSEC_PER_SEC + msec * NANOSEC_PER_SEC / 1000;
    }

#define TIMER_RELATIVETIME (0)

/// Delay the calling thread by nsecs nanoseconds.
    void thread_delay(Time::Time_t nsecs)
    {
        struct timespec rqtp;
        time2timespec(nsecs, rqtp);
        clock_nanosleep(CLOCK_REALTIME, TIMER_RELATIVETIME, &rqtp, 0);
    }

/// Sleep until the time specified
    void thread_sleep_until(Time::Time_t abstime, clockid_t clock)
    {
        struct timespec rqtp;
        time2timespec(abstime, rqtp);
        clock_nanosleep(clock, TIMER_ABSTIME, &rqtp, 0);
    }

// Output a time to ISO 8601 compliant string
    std::string isoDateTime(Time::Time_t t)
    {
        int year, month, day, hour, minute;
        double second;
        std::stringstream ss;
        calendarDate(t, year, month, day, hour, minute, second);
        ss << year << "-" << std::setw(2) << std::setfill('0') << month << "-"
           << std::setw(2) << std::setfill('0') << day
           << "T"
           << std::setw(2) << std::setfill('0')
           << hour << ":"
           << std::setw(2) << std::setfill('0') << minute << ":"
           << std::fixed << std::setw(6) << std::setfill('0') << std::setprecision(3) << second << "Z";
        return ss.str();
    }

};
