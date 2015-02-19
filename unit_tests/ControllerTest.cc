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


#include <iostream>
#include "ControllerTest.h"
#include "Controller.h"
#include "Component.h"
#include "yaml.h"

using namespace std;
using namespace YAML;

// A simple toy Component for testing
class HelloWorldComponent : public Component
{
public:
    HelloWorldComponent(string myname) : Component(myname) 
    { cout << "creating component " << myname << endl; }
    static Component *factory(string myname)
    { return new HelloWorldComponent(myname); }
    virtual ~HelloWorldComponent() 
    {  }

};
    

// test for approximate equivalent time 
void ControllerTest::test_init()
{
    Controller simple("hello_world.yaml");
    simple.add_factory_method("HelloWorldComponent", &HelloWorldComponent::factory);
    // CPPUNIT_ASSERT(simple.create_the_keymaster());
    CPPUNIT_ASSERT( simple.create_component_instances() );
}

void ControllerTest::test_component_init()
{
    shared_ptr<Component> c;
    c.reset( HelloWorldComponent::factory("test_component") );
    c->contact_keymaster();
    c->initialize_fsm();

    // The events below will come via the keymaster publisher
    // of the Controller's commands    
    CPPUNIT_ASSERT( c->get_state() == "Created" );
    CPPUNIT_ASSERT( c->process_command("register") );
    CPPUNIT_ASSERT( c->get_state() == "Standby" );
    CPPUNIT_ASSERT( c->process_command("initialize") );
    CPPUNIT_ASSERT( c->get_state() == "Ready" );   
    CPPUNIT_ASSERT( c->process_command("start") );
    CPPUNIT_ASSERT( c->get_state() == "Running" );   
    CPPUNIT_ASSERT( c->process_command("stop") ); 
    CPPUNIT_ASSERT( c->get_state() == "Ready" );
    CPPUNIT_ASSERT(!c->process_command("foobar") );
    CPPUNIT_ASSERT( c->get_state() == "Ready" );
    CPPUNIT_ASSERT( c->process_command("start") );
    CPPUNIT_ASSERT( c->get_state() == "Running" );
    CPPUNIT_ASSERT( c->process_command("error") );
    CPPUNIT_ASSERT( c->get_state() == "Ready" );
}






