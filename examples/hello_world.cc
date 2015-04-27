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

#include "Controller.h"
#include "Component.h"
#include "Keymaster.h"

using namespace std;
using namespace YAML;

void hook(void)
{
    printf("hook on tid %ld\n", pthread_self());
}

// Example trival component
class ClockComponent : public Component
{
public:
    ClockComponent(string name, string urn) : 
        Component(name, urn),
        run_thread(this, &ClockComponent::run_loop), ticks(0)
    {
        run_thread.start();
    }
    void run_loop()
    {
        while(1)
        {
            sleep(1);
            YAML::Node tm = keymaster->get("components." + my_instance_name + ".time");
            cout << "Clock says " << tm.as<string>() << endl;
            keymaster->put("components." + my_instance_name + ".time", ++ticks, true);
        }
    }
    static Component *factory(string myname,string k)
    { cout << "ClockComponent ctor" << endl; return new ClockComponent(myname, k); }
    
protected:
    Thread<ClockComponent> run_thread;
    int ticks;
};

class IndicatorComponent : public Component
{
public:
    IndicatorComponent(string name, string urn) : Component(name, urn) 
    {
        keymaster->subscribe("components.clock.time", 
                            new KeymasterMemberCB<IndicatorComponent>(this, &IndicatorComponent::report) );
    }
    void report(string yml_path, YAML::Node time)
    {
        cout << "Time now " << time.as<string>() << endl;
    }
    static Component *factory(string myname,string k)
    { cout << "IndicatorComponent ctor" << endl; return new IndicatorComponent(myname, k); }
};

class TestController : public Controller
{
public:
    TestController(string conf_file);
    
};

TestController::TestController(string conf_file) : 
    Controller(new KeymasterServer(conf_file), "control", "inproc://matrix.keymaster")
{
    add_component_factory("ClockComponent",     &ClockComponent::factory);
    add_component_factory("IndicatorComponent", &IndicatorComponent::factory);

    try { basic_init(); } catch(ControllerException e)
    {
        cout << e.what() << endl;
        throw e;
    }

    initialize(); // Sends the init event to get components initialized.    
}


int main(int argc, char **argv)
{

    ThreadBase::set_thread_create_hook(hook);

    TestController simple(string("hello_world.yaml"));
    

    // wait for the keymaster events which report components in the Standby state.
    // Probably should return if any component reports and error state????
    bool result = simple.wait_all_in_state("Standby", 1000000);
    if (!result)
    {
        cout << "initial standby state error" << endl;
    }
    simple.set_system_mode("CLOCK");

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
