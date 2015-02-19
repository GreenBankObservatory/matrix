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
#include <map>
#include <iostream>
#include <algorithm>
#include "Controller.h"
#include <yaml-cpp/yaml.h>

#include <thread>             // std::thread, std::this_thread::yield
#include <mutex>              // std::mutex
#include <condition_variable> // std::condition_variable_any

using namespace std;

Controller::Controller(string config_file) : state_condition(false)
{
    fsm.addTransition("_created", "do_initial_init", "Standby");
    fsm.addTransition("Standby",  "initialize",      "Ready");
    fsm.addTransition("Ready",    "stand_down",      "Standby");
    fsm.addTransition("Ready",    "start",           "Running");
    fsm.addTransition("Running",  "error",           "Ready");    
    fsm.addTransition("Running",  "stop",            "Ready");
    fsm.setInitialState("_created");
    
    // Temporary stand-in until Keymaster is available ...
    key_root = YAML::LoadFile(config_file);
}

Controller::~Controller()
{
}

bool Controller::create_the_keymaster()
{
    return _create_the_keymaster();
}

bool Controller::create_component_instances()
{
    return _create_component_instances();
}

bool Controller::subscribe_to_components()
{
    return _subscribe_to_components();
}

bool Controller::initialize()
{
    return _initialize();
}

bool Controller::stand_down()
{
    return _stand_down();
}

bool Controller::start()
{
    return _start();
}

bool Controller::stop()
{
    return _stop();
}

bool Controller::exit_system()
{
    return _exit_system();
}

bool Controller::send_event(string event)
{
    return _send_event(event);
}

bool Controller::wait_for_all(std::string statename, int timeout)
{
    int rtn = count(component_states.begin(), component_states.end(), statename);
    return rtn == component_states.size();
}

bool Controller::check_all_in_state(string statename)
{
    int rtn = count(component_states.begin(), component_states.end(), statename);
    return rtn == component_states.size();
}

bool Controller::_create_the_keymaster()
{
    cout << __func__ << " - not implemented" << endl;
    return false;
}

bool Controller::_create_component_instances()
{
    YAML::Node components = key_root["components"];
    cout << "Components are:" << endl;

    for (YAML::const_iterator it = components.begin(); it != components.end(); ++it)
    {
        string comp_instance_name = it->first.as<string>();
        cout << "\t" << it->first << endl;
        YAML::Node type = it->second["type"];
        
        if (!type)
        {
            cerr << "No type field for component " << comp_instance_name << endl;
            return false;
        }
        else if (factory_methods.find(type.as<string>()) == factory_methods.end())
        {
            cerr << "No factory for component of type " << type << " found in factory list" << endl;
            return false;
        }
        else
        {
            auto fmethod = factory_methods.find(type.as<string>());
            component_instances[comp_instance_name] = shared_ptr<Component>( 
                (*fmethod->second)(comp_instance_name) );
            cout << "Created component " << comp_instance_name << endl;
        }

    }
    return true;
}

bool Controller::_subscribe_to_components()
{
    for (auto c=component_instances.begin(); c!=component_instances.end(); ++c)
    {
        // for a key such that it is 'instance_name.status' and subscribe to it
        string key = c->first + ".status"; // A YAML Node?
    }
}

bool Controller::_initialize()
{
    return fsm.handle_event("initialize");
}

bool Controller::_stand_down()
{
    return fsm.handle_event("stand_down");
}

bool Controller::_start()
{
    return fsm.handle_event("start");
}

bool Controller::_stop()
{
    return fsm.handle_event("stop");
}

bool Controller::_exit_system()
{
    cout << __func__ << " - not implemented" << endl;
    return false;
}

bool Controller::_send_event(std::string event)
{
    cout << __func__ << " - not implemented" << endl;
    return false;
}

void Controller::add_factory_method(string name, Component *(*func)(string))
{
    factory_methods[name] = func;
}    



