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
//       GBT Operations
//       National Radio Astronomy Observatory
//       P. O. Box 2
//       Green Bank, WV 24944-0002 USA


#include <cstdio>
#include "DataInterfaceTest.h"
#include "DataInterface.h"
#include "ZMQDataInterface.h"
#include "yaml_util.h"

using namespace std;
using namespace mxutils;


void DataInterfaceTest::test_basic()
{
    DataSource src;
    DataSink   dst;
    string some_data;
    string data;

    some_data.resize(100);

    src.publish(some_data);
    dst.get(data);
}

void WTF(const YAML::Node &n)
{
    if (n.IsSequence() == true)
    {
        cout << "a list" << endl;
    }
    else if (n.IsMap() == true)
    {
        cout << "is a map" << endl;
    }
    else if (n.IsScalar() == true)
    {
        cout << "is a scalar" << endl;
    }
    else if (n.IsNull() == true)
    {
        cout << "is a null" << endl;
    }
    else if (n.IsDefined() == false)
    {
        cout << "is a undefined" << endl;
    }    
    else
    { 
        cout << "WTF" << endl;
    }
}

void DataInterfaceTest::test_basic_zmq()
{
    // component gets config for src:
    // "components.myc.source.ABC.protocol[inproc]",
    YAML::Node abc_node = YAML::Load("component:\n"
                                     "  nettask:\n"
                                     "    source:\n"   
                                     "      A_src:\n"
                                     "        protocol: [inproc, tcp]");

    cout << abc_node << endl;
    cout << endl;

    yaml_result protoqry = get_yaml_node("component.nettask.source.A_src", abc_node);
    if (protoqry.result != true)
    {
        cout << "bad query?" << endl;
    }
    YAML::Node protocols = protoqry.node["protocol"];
    WTF(protocols);

    // I think we expect it to be a map
    for (YAML::iterator it = protocols.begin(); it !=protocols.end(); ++it)
    //for (int i=0; i<2; ++i)
    {
        cout << "F:" << it->first << " S:" << it->second << endl;
    }
    YAML::Node ptp = protocols["protocol"];
    WTF(ptp);
    for (int i=0; i<2; ++i)
    {
        cout << ptp[i].as<string>() << endl;
    }
    #if 0
    ZMQDataSource src;
    src.bind(abc_node); // in/out after abc_node has urn binding entries
    
    ZMQDataSink   dst;
    string some_data;
    string data;

    some_data.resize(100);

    src.publish(some_data);
    dst.get(data);
    #endif
}





