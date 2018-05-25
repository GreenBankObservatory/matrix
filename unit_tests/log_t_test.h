/*******************************************************************
 *  log_t_test.h - Unit tests for the log_t logger module
 *
 *  Copyright (C) 2018 Associated Universities, Inc. Washington DC, USA.
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

#if !defined(_LOG_T_TEST_H_)
#define _LOG_T_TEST_H_

#include <cppunit/extensions/HelperMacros.h>

class log_tTest: public CppUnit::TestCase
{
    CPPUNIT_TEST_SUITE(log_tTest);
    CPPUNIT_TEST(test_logger);
    CPPUNIT_TEST(test_ostream_backend);
    CPPUNIT_TEST(test_ostream_color_backend);

    CPPUNIT_TEST_SUITE_END();

public:

    void test_logger();
    void test_ostream_backend();
    void test_ostream_color_backend();
};


#endif
