/*******************************************************************
 *  utility_test.cc - Tests the Matrix utility functions.
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

#include "utility_test.h"
#include "yaml_util.h"

#include <iostream>


using namespace std;
using namespace mxutils;

YAML::Node create_sample_yaml_node()
{
    // Start with fresh node
    YAML::Node node;
    // Add a node to the node, giving it a key
    node["components"] = YAML::Node();

    // Create a component:
    YAML::Node foo;
    // add a source
    foo["sources"] = YAML::Node();
    foo["sources"]["A"].push_back("inproc");
    foo["sources"]["A"].push_back("IPC");
    foo["sources"]["A"].push_back("TCP");

    // an alternative:
    foo["sources"]["B"] = YAML::Load("[IPC, TCP, inproc]");

    // yet another way
    vector<string> xs = {"IPC", "TCP", "inproc"};
    foo["sources"]["C"] = YAML::Node(xs);
    foo["ID"] = 0x1234;

    // Add 'foo' to 'components', giving it a key
    node["components"]["foocomponent"] = foo;
    return node;
}

void UtilityTest::test_get_yaml_node()
{
    yaml_result r;

    YAML::Node node = create_sample_yaml_node();

    r = get_yaml_node(node, "components.foocomponent.ID");

    CPPUNIT_ASSERT(r.result);
    CPPUNIT_ASSERT(r.key == "components.foocomponent.ID");
    int val = r.node.as<int>();
    CPPUNIT_ASSERT(val == 0x1234);

    r = get_yaml_node(node, "components.foocomponent.IB"); // mistake

    CPPUNIT_ASSERT(!r.result);
    CPPUNIT_ASSERT(r.key == "components.foocomponent");

    r = get_yaml_node(node, "components.faocomponent.ID");
    CPPUNIT_ASSERT(!r.result);
    CPPUNIT_ASSERT(r.key == "components");

    r = get_yaml_node(node, "camponents.foocomponent.ID");
    CPPUNIT_ASSERT(!r.result);
    CPPUNIT_ASSERT(r.key.empty());

    r = get_yaml_node(node, "components.foocomponent.ID.foo.bar");
    CPPUNIT_ASSERT(!r.result);
    CPPUNIT_ASSERT(r.key == "components.foocomponent.ID");

}

void UtilityTest::test_put_yaml_node()
{
    yaml_result r;

    YAML::Node node = create_sample_yaml_node();

    YAML::Node v = YAML::Load("1111");
    r = put_yaml_node(node, "components.foocomponent.ID", v, true);

    CPPUNIT_ASSERT(r.result);
    int val = r.node.as<int>();
    // the returned node should be what we set
    CPPUNIT_ASSERT(val == 1111);
    val = node["components"]["foocomponent"]["ID"].as<int>();
    // the root node should have been set.
    CPPUNIT_ASSERT(val == 1111);

    // replace current node with a new value
    r = put_yaml_val(node, "components.foocomponent.ID", 3.1415923);
    CPPUNIT_ASSERT(r.result);
    CPPUNIT_ASSERT(r.key == "components.foocomponent.ID");
    CPPUNIT_ASSERT(r.node.as<double>() == node["components"]["foocomponent"]["ID"].as<double>());

    // replace a non-existent node with a new value
    r = put_yaml_val(node, "components.foocomponent.PI", 3.1415923);
    CPPUNIT_ASSERT(!r.result);
    CPPUNIT_ASSERT(r.key == "components.foocomponent");
    CPPUNIT_ASSERT(!r.err.empty());

    // create a value at a new node
    r = put_yaml_val(node, "components.foocomponent.PI", 3.1415923, true);
    CPPUNIT_ASSERT(r.result);
    CPPUNIT_ASSERT(r.key == "components.foocomponent.PI");
    CPPUNIT_ASSERT(r.err.empty());
    CPPUNIT_ASSERT(r.node.as<double>() == node["components"]["foocomponent"]["PI"].as<double>());

    // create something more complex
    vector<int> xs =
        {
            1, 2, 3, 4, 5
        };

    r = put_yaml_val(node, "components.bar.quux", xs, true);
    CPPUNIT_ASSERT(r.result);
    CPPUNIT_ASSERT(r.key == "components.bar.quux");
    CPPUNIT_ASSERT(r.err.empty());
    CPPUNIT_ASSERT(xs == r.node.as<vector<int> >());
    CPPUNIT_ASSERT(xs == node["components"]["bar"]["quux"].as<vector<int> >());
}

void UtilityTest::test_delete_yaml_node()
{
    YAML::Node node = create_sample_yaml_node();
    yaml_result r;

    // should fail, there is no "bar.baz" key.
    r = delete_yaml_node(node, "components.bar.baz");
    CPPUNIT_ASSERT(!r.result);
    CPPUNIT_ASSERT(!r.err.empty());

    // delete something that does exist
    r = delete_yaml_node(node, "components.foocomponent.sources");
    CPPUNIT_ASSERT(r.result);
    CPPUNIT_ASSERT(r.err.empty());

    // this node should be gone now
    CPPUNIT_ASSERT(!node["components"]["foocomponent"]["sources"]);
}
