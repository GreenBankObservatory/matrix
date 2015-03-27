/*******************************************************************
 *  keymaster_test.cc - Tests for the keymaster
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
#include "keymaster_test.h"

using namespace std;
using namespace mxutils;


void KeymasterTest::test_keymaster()
{
    yaml_result r;
    boost::shared_ptr<KeymasterServer> km_server;

    CPPUNIT_ASSERT_NO_THROW(
        km_server.reset(new KeymasterServer("test.yaml"));
        km_server->run();
        );

    Keymaster km("inproc://matrix.keymaster");
    // get a value as a YAML::Node
    YAML::Node n;
    CPPUNIT_ASSERT_NO_THROW(n = km.get("components.nettask.source.URLs"));
    r = km.get_last_result();
    CPPUNIT_ASSERT(r.result);
    CPPUNIT_ASSERT(r.key == "components.nettask.source.URLs");

    // get a vector of strings
    vector<string> urls;
    CPPUNIT_ASSERT_NO_THROW(
        urls = km.get_as<vector<string> >("components.nettask.source.URLs")
        );

    // error: 'foo.bar.baz' doesn't exist.
    CPPUNIT_ASSERT_THROW(
        urls = km.get_as<vector<string> >("foo.bar.baz"),
        KeymasterException);
    r = km.get_last_result();
    CPPUNIT_ASSERT(r.result == false);
    CPPUNIT_ASSERT(r.err == "No such key: foo");
    CPPUNIT_ASSERT(r.key.empty());

    // Put a new value into the keymaster. But 'ID' does not exist and
    // we didn't ask that it be created.
    CPPUNIT_ASSERT_THROW(
        km.put("components.nettask.source.ID", 1234),
        KeymasterException);

    // Put a new value into the keymaster. Specify the create flag; this
    // should now work
    CPPUNIT_ASSERT_NO_THROW(
        km.put("components.nettask.source.ID", 1234, true)
        );

    // Replace an existing value in the keymaster. Now that 'ID' exists,
    // thish should not throw.
    CPPUNIT_ASSERT_NO_THROW(
        km.put("components.nettask.source.ID", 9999)
        );

    // delete a YAML::Node from the keymaster at key 'ID':
    CPPUNIT_ASSERT_NO_THROW(
        km.del("components.nettask.source.ID")
        );

    // again, check the result, if desired. If the key exists, the
    // yaml_result::result flag will be true, the key will be
    // "components.nettask.source" (because we just deleted 'ID'),
    // the error message will be blank, and the node will be the one
    // pointed to by the returned keychain.
    r = km.get_last_result();
    CPPUNIT_ASSERT(r.result);
    CPPUNIT_ASSERT(r.key == "components.nettask.source");
    CPPUNIT_ASSERT(r.err.empty());
}

template <typename T>
struct MyCallback : public KeymasterCallback
{
    MyCallback(T v)
    : data(v)
    {}

    T data;

private:
    void _call(YAML::Node n)
    {
        data = n.as<T>();
    }
};

void KeymasterTest::test_keymaster_publisher()
{
    // yaml_result r;
    // boost::shared_ptr<KeymasterServer> km_server;

    // CPPUNIT_ASSERT_NO_THROW(
    //     km_server.reset(new KeymasterServer("test.yaml"));
    //     km_server->run();
    //     );

    // Keymaster km("inproc://matrix.keymaster");

    // int val = 0;
    // MyCallback<int> cb(val);

    // km.subscribe("components.nettask.source.ID", &cb);

    // // Put a new value into the keymaster
    // km.put("components.nettask.source.ID", 1234, true);

    // CPPUNIT_ASSERT(cb.data == 1234);

    // // Replace an existing value in the keymaster
    // km.put("components.nettask.source.ID", 9999);

    // CPPUNIT_ASSERT(cb.data == 9999);

    CPPUNIT_ASSERT(1);
}
