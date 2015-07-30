/*******************************************************************
 *  TransportTest.cc - Implements the Data Interface tests.
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

#include <iostream>
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>
#include <boost/shared_ptr.hpp>

#include "Keymaster.h"
#include "yaml_util.h"
#include "matrix_util.h"
#include "TransportTest.h"
#include "TCondition.h"
#include "DataInterface.h"

using namespace std;
using namespace mxutils;
using namespace matrix;

static std::string km_urn("inproc://interface_tests.keymaster");

static std::string yaml_configuration =
    "Keymaster:\n"\
    "  URLS:\n"\
    "    Initial:\n"\
    "      - inproc://interface_tests.keymaster\n"\
    "\n"\
    "components:\n"\
    "  moby_dick:\n"\
    "    Transports:\n"\
    "      A:\n"\
    "        Specified: [rtinproc]\n"\
    "    Sources:\n"\
    "      lines: A\n";


void TransportTest::setUp()
{
    YAML::Node n = YAML::Load(yaml_configuration);
    _kms.reset(new KeymasterServer(n));
    _kms->run();
    _km.reset(new Keymaster(km_urn));
    TestCase::setUp();
}

void TransportTest::tearDown()
{
    _km.reset();
    _kms.reset();
    TestCase::tearDown();
}

void TransportTest::test_data_source_create()
{
    vector<string> tr = {"rtinproc"};
    _km->put("components.moby_dick.Transports.A.Specified", tr);
    shared_ptr<DataSource<double> > dsource;
    CPPUNIT_ASSERT_NO_THROW(dsource.reset(new DataSource<double>(km_urn, "moby_dick", "lines")));
    shared_ptr<DataSource<string> > ssource;
    CPPUNIT_ASSERT_NO_THROW(ssource.reset(new DataSource<string>(km_urn, "moby_dick", "lines")));
    dsource.reset();
    ssource.reset();
}

void TransportTest::test_data_sink_create()
{
    shared_ptr<DataSink<double, select_only> > dsink;
    CPPUNIT_ASSERT_NO_THROW(dsink.reset(new DataSink<double, select_only>(km_urn)));

    shared_ptr<DataSink<string, select_only> > ssink;
    CPPUNIT_ASSERT_NO_THROW(ssink.reset(new DataSink<string, select_only>(km_urn)));

    // the DataSinks should not throw even if there is no keymaster
    _kms.reset();

    shared_ptr<DataSink<double, select_only> > dnksink;
    CPPUNIT_ASSERT_NO_THROW(dnksink.reset(new DataSink<double, select_only>(km_urn)));

    shared_ptr<DataSink<string, select_only> > snksink;
    CPPUNIT_ASSERT_NO_THROW(snksink.reset(new DataSink<string, select_only>(km_urn)));

    dsink.reset();
    ssink.reset();
    dnksink.reset();
    snksink.reset();
}

void TransportTest::do_the_transaction(string transport)
{
    double d_sent = 3.14159, d_recv;
    string s_sent = "Call me Ishmael.", s_recv;

    vector<string> tr = {transport};
    _km->put("components.moby_dick.Transports.A.Specified", tr);

    shared_ptr<DataSource<double> > dsource(new DataSource<double>(km_urn, "moby_dick", "lines"));
    shared_ptr<DataSink<double, select_only> > dsink((new DataSink<double, select_only>(km_urn)));

    dsink->connect("moby_dick", "lines");
    do_nanosleep(0, 1000000);
    dsource->publish(d_sent);

    // try_get() returns false if there is nothing, true if there is.
    int i = 0;

    while (!dsink->try_get(d_recv))
    {
        do_nanosleep(0, 100000);

        if (i++ == 100)
        {
            break;
        }
    }

    CPPUNIT_ASSERT_DOUBLES_EQUAL(d_sent, d_recv, 0.000001);

    // this wipes out any '.AsConfigured' entries in the keymaster
    // for the transport used here.
    dsource.reset();
    dsink.reset();

    shared_ptr<DataSource<string> > ssource(new DataSource<string>(km_urn, "moby_dick", "lines"));
    shared_ptr<DataSink<string, select_only> > ssink((new DataSink<string, select_only>(km_urn)));

    ssink->connect("moby_dick", "lines");
    do_nanosleep(0, 1000000);
    ssource->publish(s_sent);
    i = 0;

    while (!ssink->try_get(s_recv))
    {
        do_nanosleep(0, 100000);

        if (i++ == 100)
        {
            break;
        }
    }

    CPPUNIT_ASSERT_EQUAL(d_sent, d_recv);
    dsource.reset();
    dsink.reset();
    ssource.reset();
    ssink.reset();
}

void TransportTest::test_inproc_publish()
{
    do_the_transaction("inproc");
}

void TransportTest::test_ipc_publish()
{
    do_the_transaction("ipc");
}

void TransportTest::test_tcp_publish()
{
    do_the_transaction("tcp");
}

void TransportTest::test_rtinproc_publish()
{
    do_the_transaction("rtinproc");
}
