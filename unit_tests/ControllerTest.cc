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
#include <yaml-cpp/yaml.h>

#include "ControllerTest.h"
#include "Controller.h"
#include "Component.h"

using namespace std;
using namespace YAML;

// A simple toy Component for testing
class HelloWorldComponent : public Component
{
public:
    HelloWorldComponent(string myname, string k_url) : Component(myname, k_url)
    { /*cout << "creating component " << myname << endl;*/ }
    static Component *factory(string myname, string k_url)
    { return new HelloWorldComponent(myname, k_url); }
    virtual ~HelloWorldComponent()
    {  }

};


// test for approximate equivalent time
void ControllerTest::test_init()
{
    Controller simple("hello_world.yaml");
    simple.add_component_factory("HelloWorldComponent", &HelloWorldComponent::factory);
    
    CPPUNIT_ASSERT( simple.basic_init());
    CPPUNIT_ASSERT( simple.initialize());
    
    CPPUNIT_ASSERT( simple.wait_all_in_state("Standby", 1000000) );
    CPPUNIT_ASSERT( simple.set_system_mode("default") );    
    CPPUNIT_ASSERT( simple.ready());
    CPPUNIT_ASSERT( simple.wait_all_in_state("Ready", 1000000) );
    CPPUNIT_ASSERT( simple.start());
    CPPUNIT_ASSERT( simple.wait_all_in_state("Running", 1000000) );
    CPPUNIT_ASSERT( simple.stop());
    CPPUNIT_ASSERT( simple.wait_all_in_state("Ready", 1000000) );
    
    // In order to change system mode we must first revert to stanby
    CPPUNIT_ASSERT( simple.standby());
    CPPUNIT_ASSERT( simple.wait_all_in_state("Standby", 1000000) );
    
    CPPUNIT_ASSERT( simple.set_system_mode("VEGAS_LBW") );
    CPPUNIT_ASSERT( simple.ready());
    CPPUNIT_ASSERT( simple.wait_all_in_state("Ready", 1000000) );
    CPPUNIT_ASSERT( simple.start());
    
    // CPPUNIT_ASSERT( !simple.set_system_mode("default") );
    
    CPPUNIT_ASSERT( simple.wait_all_in_state("Running", 1000000) );
    CPPUNIT_ASSERT( simple.stop());
    CPPUNIT_ASSERT( simple.wait_all_in_state("Ready", 1000000) );
    CPPUNIT_ASSERT( simple.standby());
    CPPUNIT_ASSERT( simple.wait_all_in_state("Standby", 1000000) );
    
    simple.terminate();
    
    timespec slp;
    slp.tv_sec = 1;
    slp.tv_nsec = 0;
    fflush(stdout);
    //nanosleep(&slp, 0);
    //simple.terminate();
    cout << "terminated" << endl << endl;
}

void ControllerTest::test_component_init()
{
#if 0
    // This needs to be merged with controller tests
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
#endif
}


