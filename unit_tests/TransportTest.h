/*******************************************************************
 *  TransportTest.h - Tests the various transports, and DataSource and
 *  DataSink.
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

#if !defined(_TRANSPORTTEST_H_)
#define _TRANSPORTTEST_H_

#include <cppunit/extensions/HelperMacros.h>

class KeymasterServer;
class Keymaster;

class TransportTest : public CppUnit::TestCase
{
    CPPUNIT_TEST_SUITE(TransportTest);
    CPPUNIT_TEST(test_data_source_create);
    CPPUNIT_TEST(test_data_sink_create);
    CPPUNIT_TEST(test_inproc_publish);
    CPPUNIT_TEST(test_ipc_publish);
    CPPUNIT_TEST(test_tcp_publish);
    CPPUNIT_TEST(test_rtinproc_publish);
    CPPUNIT_TEST_SUITE_END();

    std::shared_ptr<KeymasterServer> _kms;
    std::shared_ptr<Keymaster> _km;

    void do_the_transaction(std::string transport);
    
public:
    void setUp();
    void tearDown();

    void test_data_source_create();
    void test_data_sink_create();
    void test_inproc_publish();
    void test_ipc_publish();
    void test_tcp_publish();
    void test_rtinproc_publish();
};

#endif
