/*******************************************************************
 *  matrix_util.cc - some useful utilities
 *
 *  Copyright (C) 2015 Associated Universities, Inc. Washington DC, USA.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Correspondence concerning GBT software should be addressed as follows:
 *  GBT Operations
 *  National Radio Astronomy Observatory
 *  P. O. Box 2
 *  Green Bank, WV 24944-0002 USA
 *
 *******************************************************************/

#include "matrix/matrix_util.h"
#include <set>
#include <iostream>
#include <iomanip>
#include <string>
#include <algorithm>
#include <sys/time.h>
#include <cmath>

using namespace std;

namespace mxutils
{

/********************************************************************
 * ToHex(const string &s, bool upper_case)
 *
 * Converts a binary string to the kind of hex byte output you'd see in
 * a hex editor. See
 * http://stackoverflow.com/questions/9621893/c-read-binary-file-and-convert-to-hex
 *
 * @param const string &s: the byte buffer.
 * @param bool upper_case: Set to true if upper case hex output desired.
 *
 * @return A std::string, the hex string.
 *
 *******************************************************************/

string ToHex(const string &s, bool upper_case, size_t max_len)
{
    ostringstream ret;
    size_t max;

    if (max_len)
    {
        max = min(max_len, s.length());
    }
    else
    {
        max = s.length();
    }

    for (string::size_type i = 0; i < max; ++i)
    {
        int z = s[i]&0xff;
        ret << std::hex << std::setfill('0') << std::setw(2)
            << (upper_case ? std::uppercase : std::nouppercase) << z << " ";
    }

    if (max_len)
    {
        ret << "... (len=" << s.length() << ")";
    }

    return ret.str();
}

/**
 * This utility will call nanosleep with the proper fields. It will
 * resume sleeping after being interrupted by a signal.
 *
 * Note: Don't use this if you want your nanosleep to be interrupted
 * by a signal!
 *
 * @param seconds: int, number of seconds to sleep
 *
 * @param nanoseconds: int, number of nanoseconds to sleep.
 *
 */

    void do_nanosleep(int seconds, int nanoseconds)

    {
        timespec req, rem;
        req.tv_sec = (time_t)seconds;
        req.tv_nsec = (long)nanoseconds;

        // nanosleep could be interupted by signal.
        while (nanosleep(&req, &rem) == -1)
        {
            req.tv_sec = rem.tv_sec;
            req.tv_nsec = rem.tv_nsec;
        }
    }

/**
 * operator < for struct timeval. Easily compare if one timeval is
 * less than another.
 *
 * example:
 *
 *      timeval a, b;
 *      ...
 *      gettimeofday(&b, NULL);
 *
 *      if (b < a)
 *      {
 *       ...
 *      }
 * @param lhs: the left-hand side operand
 *
 * @param rhs: the righ-hand side operand
 *
 * @return a boolean value, true if lhs is < rhs.
 *
 */

    bool operator<(timeval &lhs, timeval &rhs)
    {
        if (lhs.tv_sec < rhs.tv_sec)
        {
            return true;
        }
        else if (lhs.tv_sec == rhs.tv_sec)
        {
            return lhs.tv_usec < rhs.tv_usec;
        }

        return false;
    }

/**
 * operator+() adds two timevals together, returning a third, which
 * will correctly hold the sum of the two operands.
 *
 * @param lhs, holding the left-hand operand (in infix notation).
 *
 * @param rhs, holding the right-hand operand.
 *
 * @return a timeval with the correct time sum.
 *
 */

    timeval operator+(timeval lhs, timeval rhs)
    {
        timeval t = {0, 0};

        t.tv_usec = lhs.tv_usec + rhs.tv_usec;

        while (t.tv_usec > 999999)
        {
            t.tv_sec++;
            t.tv_usec -= 1000000;
        }

        t.tv_sec += lhs.tv_sec + rhs.tv_sec;
        return t;
    }

/**
 * operator+() for a timeval and a double. Adds a floating point
 * representation of a number of seconds together with a time
 * represented by a timeval, returning the sum as a timeval.
 *
 * example:
 *
 *      timeval now, sum;
 *      gettimeofday(&now, NULL);
 *      sum = now + 1.2; // adds 1.2 seconds to the value of 'now'
 *                       // placing the result in 'sum'
 *
 * @param lhs: the left-hand side operand, as a timeval value
 *
 * @param rhs: the right-hand side operand, in seconds
 *
 * @return a timeval, the sum of the two values.
 *
 */

    timeval operator+(timeval lhs, double rhs)
    {
        double x, ipart;
        timeval t;

        x = modf(rhs, &ipart);
        t.tv_usec = (int64_t)(x * 1e6);
        t.tv_sec = ipart;

        return lhs + t;
    }

/**
 * operator-() for two timevals. Will compute the difference of two
 * timevals, returning a timeval == {0, 0} if the operation would
 * result in a negative timeval.
 *
 * @param lhs: the left-hand operand
 *
 * @param rhs: the right-hand operand
 *
 * @return the difference between the two timevals, or a timeval ==
 * {0, 0} if the difference would be negative (i.e. rhs > lhs).
 *
 */

    timeval operator-(timeval lhs, timeval rhs)
    {
        timeval t = { 0, 0 };

        if (rhs < lhs)
        {

            if (lhs.tv_usec < rhs.tv_usec)
            {
                lhs.tv_sec--;
                lhs.tv_usec += 1000000;
            }

            t.tv_usec = lhs.tv_usec - rhs.tv_usec;
            t.tv_sec = lhs.tv_sec - rhs.tv_sec;
        }

        return t;
    }

/**
 * operator<< extractor for timeval.
 *
 * example:
 *
 *      timeval t = {100, 500000};
 *
 *      cout << t << endl;
 *
 *  output:
 *       { tv_sec: 100, tv_usec 500000 }
 *
 * @param t: the timeval to output
 *
 * @return an ostream reference (for chaining)
 *
 */

    ostream & operator<<(ostream &os, const timeval &t)
    {
        os << "{ tv_sec: " << t.tv_sec
           << ", tv_usec: " << t.tv_usec
           << " }";
        return os;
    }
}
