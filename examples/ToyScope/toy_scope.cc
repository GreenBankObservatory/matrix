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

#include "matrix/Architect.h"
#include "matrix/Component.h"
#include "matrix/Keymaster.h"
#include "ExSignalGenerator.h"
#include "ExAccumulator.h"
#include "ExProcessor.h"

#define dbprintf if(true) printf

using namespace std;
using namespace YAML;
using namespace matrix;

int main(int argc, char **argv)
{

    // Make the Name to factory binding for the components used in this example
    Architect::add_component_factory("SignalGenerator",     &ExSignalGenerator::factory);
    Architect::add_component_factory("Accumulator",         &ExAccumulator::factory);
    Architect::add_component_factory("Processor",           &ExProcessor::factory);

    Architect::create_keymaster_server("config.yaml");
    Architect simple("control", "inproc://matrix.keymaster");
    simple.basic_init();
    simple.initialize();
    
    dbprintf("waiting for components to standby\n");
    // wait for the keymaster events which report components in the Standby state.
    // Probably should return if any component reports and error state????
    bool result = simple.wait_all_in_state("Standby", 1000000);
    if (!result)
    {
        cout << "initial standby state error" << endl;
    }
    // Setup for the default configuration
    dbprintf("setting mode to default\n");
    simple.set_system_mode("default");

    // All Components now in standby. 
    // In this example, I go ahead and get things running by issuing 
    // a 'get_ready' event. The result should be the *active* components 
    // changing to the Ready state
    simple.ready();
    dbprintf("waiting for components to ready\n");
    result = simple.wait_all_in_state("Ready", 1000000);
    if (!result)
    {
        cout << "initial standby state error" << endl;
    }
    dbprintf("starting for components\n");
    simple.start();
    dbprintf("waiting for components to ready\n");
    result = simple.wait_all_in_state("Running", 1000000);
    if (!result)
    {
        cout << "initial standby state error" << endl;
    }
    dbprintf("components ready\n");
    // I don't have anything for main to do, so just loop
    // while the rest of the system operates.
    //            
    while(true) 
    {
        dbprintf("sleep\n");
        sleep(10);
    }

    return 0;
}
