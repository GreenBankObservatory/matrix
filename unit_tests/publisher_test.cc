/*******************************************************************
 *  publisher_test.cc - Test the Publisher
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

#include "publisher_test.h"
#include "Publisher.h"
#include "zmq_util.h"
#include "ZMQContext.h"

#include <boost/shared_ptr.hpp>
#include <cstdio>

using namespace std;
using namespace mxutils;

void PublisherTest::test_publish_data()
{
    shared_ptr<Publisher> pub;
    zmq::context_t &ctx = ZMQContext::Instance()->get_context();
    zmq::socket_t pub_client(ctx, ZMQ_SUB);

    CPPUNIT_ASSERT_NO_THROW_MESSAGE("INPROC Publisher construct",
                                    pub.reset(new Publisher("foo", Publisher::INPROC)));

    // pub is running, subscribe:
    string key = "cat";
    string data = "meow";

    pub_client.connect("inproc://foo.publisher");
    pub_client.setsockopt(ZMQ_SUBSCRIBE, (const void *)key.c_str(), key.length());

    CPPUNIT_ASSERT_NO_THROW_MESSAGE("INPROC Publishing data", pub->publish_data(key, data));

    string recv_data;
    z_recv(pub_client, recv_data);
    CPPUNIT_ASSERT(recv_data == key);   // first item is key
    z_recv(pub_client, recv_data);
    CPPUNIT_ASSERT(recv_data == data);  // second is the payload.

    CPPUNIT_ASSERT_NO_THROW_MESSAGE("INPROC Publisher destruct",
                                    pub.reset());
}

void PublisherTest::test_create_publisher()
{
    shared_ptr<Publisher> pub;

    CPPUNIT_ASSERT_NO_THROW_MESSAGE("INPROC Publisher construct",
                                    pub.reset(new Publisher("foo", Publisher::INPROC)));
    CPPUNIT_ASSERT_NO_THROW_MESSAGE("INPROC Publisher destruct",
                                    pub.reset());

    CPPUNIT_ASSERT_NO_THROW_MESSAGE("IPC Publisher construct",
                                    pub.reset(new Publisher("foo", Publisher::IPC)));
    CPPUNIT_ASSERT_NO_THROW_MESSAGE("IPC Publisher destruct",
                                    pub.reset());

    CPPUNIT_ASSERT_NO_THROW_MESSAGE("TCP Publisher construct",
                                    pub.reset(new Publisher("foo", Publisher::TCP, 5555)));
    CPPUNIT_ASSERT_NO_THROW_MESSAGE("TCP Publisher destruct",
                                    pub.reset());

    CPPUNIT_ASSERT_NO_THROW_MESSAGE("All transports Publisher construct",
                                    pub.reset(new Publisher("foo",
                                                            Publisher::INPROC | Publisher::IPC | Publisher::TCP,
                                                            5555)));
    CPPUNIT_ASSERT_NO_THROW_MESSAGE("All transports Publisher destruct",
                                    pub.reset());
}
