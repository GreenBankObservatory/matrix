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
//       GBT Operations
//       National Radio Astronomy Observatory
//       P. O. Box 2
//       Green Bank, WV 24944-0002 USA


#include <cstdio>
#include "TimeTest.h"
#include "Time.h"
#include <sys/time.h>
#include <math.h>

using namespace std;
using namespace Time;

// test for approximate equivalent time 
void TimeTest::test_getUTC()
{
    Time_t t;
    timeval tv;
    uint64_t seconds;
    
    t = getUTC();
    seconds = t/(1000000000LL);
    gettimeofday(&tv, 0);
    
    CPPUNIT_ASSERT( labs(seconds - tv.tv_sec) < 2 );
}

void TimeTest::test_conversions()
{
    // A time at midnight of MJD 55555
    tm gmt;
    timespec ts;
    timeval tv;
    unsigned int mjd,msec;
    double secs;
    Time_t T1, T2;
    
    tv.tv_sec = (365*3 + 31+5)*86400 + 9*3600; // seconds at Feb 5 1973, 9:00:00.5 UTC
    tv.tv_usec = 500000;
    
    ts.tv_sec = tv.tv_sec;
    ts.tv_nsec = tv.tv_usec * 1000;
    
    T1 = timeval2Time(tv);
    T2 = timespec2Time(ts);
    
    CPPUNIT_ASSERT(T1 == T2);

    int yr, month, dayom, hour, mins;
    
    calendarDate(T1, yr, month, dayom, hour, mins, secs);
    // printf("%d / %d / %d  %d:%d:%f\n", yr, month, dayom, hour, mins, secs);
    CPPUNIT_ASSERT(yr == 1973);
    CPPUNIT_ASSERT(month == 2);
    CPPUNIT_ASSERT(dayom == 5);
    CPPUNIT_ASSERT(hour == 9);
    CPPUNIT_ASSERT(mins == 0);
    CPPUNIT_ASSERT(0.5 == secs);
    
    // Compare calendarDate against gmtime for right now
    T1 = getUTC();
    time2timeval(T1, tv);
    
    struct tm result;
    gmtime_r(&tv.tv_sec, &result);
    calendarDate(T1, yr, month, dayom, hour, mins, secs);
    
    CPPUNIT_ASSERT(yr == result.tm_year + 1900);
    CPPUNIT_ASSERT(month == result.tm_mon + 1);
    CPPUNIT_ASSERT(dayom == result.tm_mday);
    CPPUNIT_ASSERT(hour == result.tm_hour);
    CPPUNIT_ASSERT(mins == result.tm_min);
    CPPUNIT_ASSERT((int)secs == result.tm_sec);
           
    T1 = timeStamp2Time(50000, 20000);
    time2TimeStamp(T1, mjd, msec);
    time2TimeStamp(T1, mjd, secs);
    CPPUNIT_ASSERT(mjd == 50000);
    CPPUNIT_ASSERT( fabs((secs*1E3) - msec) < 1 );
    
}






