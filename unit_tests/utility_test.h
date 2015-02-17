/*******************************************************************
 *  utility_test.h - Tests functions in the mxutil namespace.
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

#if !defined(_UTILITY_TEST_H_)
#define _UTILITY_TEST_H_

#include <cppunit/extensions/HelperMacros.h>

class UtilityTest : public CppUnit::TestCase
{
    CPPUNIT_TEST_SUITE(UtilityTest);
    CPPUNIT_TEST(test_get_yaml_node);
    CPPUNIT_TEST(test_put_yaml_node);
    CPPUNIT_TEST(test_delete_yaml_node);

    CPPUNIT_TEST_SUITE_END();

public:
    void test_get_yaml_node();
    void test_put_yaml_node();
    void test_delete_yaml_node();
};

#endif
