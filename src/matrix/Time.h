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

#ifndef Time_h
#define Time_h
#include <cstdint>
#include <ctime>
#include <ostream>

/// Time class and definitions
/// Basis:
/// =====
/// The basis for the Time type is integral nanoseconds since 1970
/// This representation is valid until a period which
/// exceeds our expected lifetimes.
///
/// The implementation is an  64 bit unsigned long long, and Time
/// is on most 64 bit systems a synonym for the time_t type.
///
struct timeval;
struct timespec;

namespace Time
{
    typedef uint64_t Time_t;

    extern clockid_t default_clock;
    void set_default_clock(clockid_t clkid);

    Time_t getUTC(clockid_t clk = Time::default_clock);
    Time_t timespec2Time(const timespec &ts);
    Time_t timeval2Time(const timeval &ts);
    void   time2timespec(const Time_t, timespec &);
    void   time2timeval(const Time_t, timeval &);

    /// @return the integral number of days since MJD epoch
    int    MJD(const Time_t);
    /// @return the floating point number of days since MJD epoch.
    double DMJD(const Time_t);
    /// @return the Time_t for the floating point MJD (inverse of DMJD())
    Time_t dmjd2Time(double mjd);

    Time_t timeStamp2Time(uint32_t mjd, uint32_t msec);
    void   time2TimeStamp(const Time_t, uint32_t &mjd, uint32_t &msec_since_midnight);
    void   time2TimeStamp(const Time_t, uint32_t &mjd, double   &msec_since_midnight);

    /// An analog to gmtime().
    /// @return true if calculation succeeded.
    bool   calendarDate(const Time_t, int &year,   int &month, int &dayofmonth,
                      int  &hour, int &minute, double &sec);

    /// @return the Time_t for the year, month, calendar day and milliseconds since midnight
    /// Note: limitation of 1msec resolution
    Time_t date2Time(const int year, const int month, const int dayofmonth,
                   const int msec_since_midnight = 0);

    /// Delay the calling thread by nsecs nanoseconds.
    void thread_delay(Time::Time_t nsecs);

    /// Sleep until the time specified
    void thread_sleep_until(Time::Time_t abstime, clockid_t clk=Time::default_clock);

    /// return an ISO 8601 representation string of a Time_t
    std::string isoDateTime(Time_t);

    const Time_t TM_ONE_SEC = 1000000000LL;
};





#endif
