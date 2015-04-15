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

#include "Time.h"
#include <time.h>
#include <sys/time.h>

#ifndef NANOSEC_PER_SEC
#define NANOSEC_PER_SEC (1000000000LL)
#define NANOSEC_PER_DAY (86400000000000LL)
#endif

extern "C" void  __Matrix__Time__(...)
{
}

namespace Time
{
#ifdef __XENO__
     // xenomai's ntp adjusted RT clock
    // #define USE_THE_CLOCK 42
    #define USE_THE_CLOCK CLOCK_REALTIME
#else
    // Use the standard (also ntp tempered) clock
    #define USE_THE_CLOCK CLOCK_REALTIME
#endif
                  
Time_t getUTC()
{
    timespec ts;
    clock_gettime(USE_THE_CLOCK, &ts);
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
Time_t TimeStamp2Time(unsigned int mjd, unsigned int msec)
{
    Time_t t = ((Time_t)(mjd - MJD_1970_EPOCH)) * NANOSEC_PER_DAY;
    t += ((Time_t)(msec)) * 1000000LL;
    return (t);
}

// Same restriction as above, truncates precision down to millisecond level
void  Time2TimeStamp(const Time_t t, unsigned int *mjd, unsigned int *msec)
{
    *mjd  = MJD(t);
    *msec = (unsigned int)((t/1000000LL)%86400000LL);
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

}
