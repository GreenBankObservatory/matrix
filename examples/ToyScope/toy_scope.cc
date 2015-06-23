// ======================================================================
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
//  GBT Operations
//  National Radio Astronomy Observatory
//  P. O. Box 2
//  Green Bank, WV 24944-0002 USA

#include <string>
#include <iostream>
#include <yaml-cpp/yaml.h>

#include "Architect.h"
#include "Component.h"
#include "Keymaster.h"
#include "ExSignalGenerator.h"
#include "ExAccumulator.h"
#include "ExProcessor.h"

using namespace std;
using namespace YAML;

class TestArchitect : public Architect
{
public:
    TestArchitect();
    
};

TestArchitect::TestArchitect() : 
    Architect("control", "inproc://matrix.keymaster")
{
    add_component_factory("SignalGenerator",     &ExSignalGenerator::factory);
    add_component_factory("Accumulator",         &ExAccumulator::factory);
    add_component_factory("Processor",           &ExProcessor::factory);

    try { basic_init(); } catch(ArchitectException e)
    {
        cout << e.what() << endl;
        throw e;
    }

    initialize(); // Sends the init event to get components initialized.    
}


int main(int argc, char **argv)
{

    Architect::create_keymaster_server("config.yaml");
    TestArchitect simple;
    

    // wait for the keymaster events which report components in the Standby state.
    // Probably should return if any component reports and error state????
    bool result = simple.wait_all_in_state("Standby", 1000000);
    if (!result)
    {
        cout << "initial standby state error" << endl;
    }
    simple.set_system_mode("default");

    // Everybody now in standby. Get things running by issuing a start event.
    simple.ready();
    result = simple.wait_all_in_state("Ready", 1000000);
    if (!result)
    {
        cout << "initial standby state error" << endl;
    }
    simple.start();
    result = simple.wait_all_in_state("Running", 1000000);
    if (!result)
    {
        cout << "initial standby state error" << endl;
    }
            
    while(true) 
        sleep(10);
    // Normal app would do something here.
    // tell the components to stop (They should go back to the Ready state.)
    simple.stop();
    result = simple.wait_all_in_state("Ready", 1000000);
    if (!result)
    {
        cout << "initial standby state error" << endl;
    }    
    sleep(1);
    return 0;
}
