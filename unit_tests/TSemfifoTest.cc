/*******************************************************************
 *  TSemfifoTest.cc - Unit tests for Semaphore FIFO class tsemfifo<>
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

#include "TSemfifoTest.h"
#include "tsemfifo.h"

using namespace std;
using namespace Time;


/**
 * Tests for size count. 'size' refers to the number of items placed
 * in the FIFO that have not yet been read out.
 *
 */

void TSemfifoTest::test_size()
{
    tsemfifo<int> fifo;
    int in, out;
    
    CPPUNIT_ASSERT(fifo.size() == 0);
    in = 5;
    fifo.put(in);
    in = 10;
    fifo.put(in);
    CPPUNIT_ASSERT(fifo.size() == 2);
    fifo.get(out);
    CPPUNIT_ASSERT(fifo.size() == 1);
}

/**
 * Tests the tsemfifo 'get' functionality. There are 3 get functions:
 * get(), which blocks indefinitely until something becomes available,
 * and returns immediately if something is available; `try_get()`
 * which never blocks, returning 'true' or 'false' depending on
 * whether there is data available; and `timed_get()`, which behaves
 * just as `get()` does except it returns 'false' if there is no data
 * and a time-out period expires.
 *
 */

void TSemfifoTest::test_get()
{
    tsemfifo<int> fifo;
    int in, out;
    
    in = 5;
    fifo.put(in);
    fifo.get(out);
    CPPUNIT_ASSERT(in == out);
    CPPUNIT_ASSERT(fifo.try_get(out) == false); // should be empty
    in = 3;
    fifo.put(in);
    CPPUNIT_ASSERT(fifo.try_get(out) == true);  // should have data
    CPPUNIT_ASSERT(in == out);                   // data should be 3

    Time_t to = 5000000;
    Time_t start = getUTC();
    CPPUNIT_ASSERT(fifo.timed_get(out, to) == false);
    Time_t diff = getUTC() - start;
    cout << endl << "diff = " << diff << endl;
    cout << "to = " << to << endl;
    // Actual time will be greater than time-out, depending on the
    // machine this is being run on. Allowing for 10% more than the
    // time-out seems amply reasonable.
    CPPUNIT_ASSERT((diff - to) < (to / 10));
    in = 23;
    fifo.put(in);
    start = getUTC();
    CPPUNIT_ASSERT(fifo.timed_get(out, to) == true);
    diff = getUTC() - start;
    CPPUNIT_ASSERT(out == in);
    cout << endl << "diff = " << diff << endl;
    cout << "to = " << to << endl;
    // we don't know exactly how long it takes to do this, bc of
    // machine differences, but it should take significantly less time
    // than the actual time-out.
    CPPUNIT_ASSERT(diff < (to / 1000));
}

    
    
