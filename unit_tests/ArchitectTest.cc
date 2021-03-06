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

#include "ArchitectTest.h"
#include "matrix/Architect.h"
#include "matrix/Component.h"

using namespace std;
using namespace YAML;
using namespace matrix;

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
void ArchitectTest::test_init()
{
    Architect::add_component_factory("HelloWorldComponent", &HelloWorldComponent::factory);
    Architect::create_keymaster_server("hello_world.yaml");
    Architect simple("control", "inproc://matrix.keymaster");
    
    CPPUNIT_ASSERT( simple.basic_init());

    CPPUNIT_ASSERT( simple.initialize());
    
    CPPUNIT_ASSERT( simple.wait_all_in_state("Standby", 100000000) );

    // Configure the component set for the 'default' mode.    
    CPPUNIT_ASSERT( simple.set_system_mode("default") );
       
    CPPUNIT_ASSERT( simple.ready());
    
    CPPUNIT_ASSERT( simple.wait_all_in_state("Ready", 1000000) );
    // poke commands in using direct keymaster puts. In a loop to
    // give it some more exercise.
    unique_ptr<Keymaster> km(new Keymaster("inproc://matrix.keymaster"));

    for (int j=0; j<20; ++j) 
    {    
        km->put("architect.control.command", "start", true);
        CPPUNIT_ASSERT( simple.wait_all_in_state("Running", 1000000) );

        km->put("architect.control.command", "stop", true);
        CPPUNIT_ASSERT( simple.wait_all_in_state("Ready", 1000000) );
    }    
    
    simple.start();
    CPPUNIT_ASSERT( simple.wait_all_in_state("Running", 20000000) );
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
    // Time::thread_delay(200000000000);
    
    // simple.terminate();
    Architect::destroy_keymaster_server();
}


