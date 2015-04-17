/*******************************************************************
 *  main.cc -- test keymaster app.
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

#include "DataInterface.h"
#include "Keymaster.h"
#include "ZMQContext.h"
#include "zmq_util.h"

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <yaml-cpp/yaml.h>
#include <boost/algorithm/string.hpp>

#include <time.h>

using namespace std;
using namespace mxutils;

int main(int argc, char *argv[])

{
    const char *msg = "Hello World!";

    KeymasterServer kms("test.yaml");
    kms.run();

    char ch;

    while ((ch = getchar()) != 'Q');

    DataSource ds_log("Log", "inproc://matrix.keymaster", "nettask");
    DataSource ds_data("Data", "inproc://matrix.keymaster", "nettask");
    ds_data.publish(msg, strlen(msg));
    ds_log.publish("Hi there!");

    // receive the data. No data sink yet, so do it bare.
    Keymaster km("inproc://matrix.keymaster");
    string url = km.get_as<string>("components.nettask.Transports.A.AsConfigured.0");
    cout << url << endl;

    zmq::context_t &ctx = ZMQContext::Instance()->get_context();
    zmq::socket_t pipe(ctx, ZMQ_SUB);
    pipe.connect(url.c_str());
    pipe.setsockopt(ZMQ_SUBSCRIBE, "Log", 3);
    pipe.setsockopt(ZMQ_SUBSCRIBE, "Data", 4);
    string key;
    vector<string> data;
    z_recv(pipe, key);
    z_recv_multipart(pipe, data);
    cout << "Key = " << key << endl;

    for (vector<string>::iterator i = data.begin(); i != data.end(); ++i)
    {
        cout << *i << endl;
    }

    z_recv(pipe, key);
    z_recv_multipart(pipe, data);

    cout << "Key = " << key << endl;

    for (vector<string>::iterator i = data.begin(); i != data.end(); ++i)
    {
        cout << *i << endl;
    }

    return 1;
}
