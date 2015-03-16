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
#include "ThreadLock.h"

using namespace std;

Controller::Controller(string config_file) : state_condition(bool())
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

bool Controller::emit_initialize()
{
    return _emit_initialize();
}

bool Controller::emit_stand_down()
{
    return _emit_stand_down();
}

bool Controller::emit_start()
{
    return _emit_start();
}

bool Controller::emit_stop()
{
    return _emit_stop();
}

bool Controller::exit_system()
{
    return _exit_system();
}

bool Controller::send_event(string event)
{
    return _send_event(event);
}

struct Is_in_state
{
    Is_in_state(string s) : compare_state(s) {}
    bool operator()(pair<string,string> p) const { return p.second != compare_state; }
    string compare_state;
};

bool Controller::check_all_in_state(string statename)
{
    Is_in_state in_state(statename);
    auto rtn = find_if(component_states.begin(), component_states.end(), in_state); 
    return rtn == component_states.end();
}

bool Controller::wait_all_in_state(string statename, int timeout)
{
    
    state_condition.lock();
    while ( !check_all_in_state(statename) )
    {
        state_condition.wait_locked_with_timeout(timeout);
    }
    state_condition.unlock();
    
    return true;
}

bool Controller::_create_the_keymaster()
{
    cout << __func__ << " - not implemented" << endl;
    // for now we emulate via a simple string/string map
    return true;
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
            cout << "Created component named " << comp_instance_name << endl;
        }
        ThreadLock<decltype(component_states) > l(component_states);
        component_states[comp_instance_name] = "Created";
        l.unlock();
        
        ThreadLock<decltype(component_status) > k(component_status);
        component_status[comp_instance_name] = "None";
        k.unlock();
    }
    return true;
}

bool Controller::_subscribe_to_components()
{
    // The keymaster will publish all updates, and the controller should
    // already have a publisher connection. Here we look for the .status
    // and .command entries.
    for (auto c=component_instances.begin(); c!=component_instances.end(); ++c)
    {
        // for a key such that it is 'instance_name.status' and subscribe to it
        string key = c->first + ".status"; // A YAML Node?
    }
}

bool get_last_path(string x, string &last_str)
{
    size_t p = x.find_last_of(".");
    if (p == string::npos)
    {
        return false;
    }
    last_str = x.substr(p);
    return true;
}

// A callback called when the keymaster publish's a components state change.
// Not sure about the type of the component arg. 
// I am assuming there will be a last path component of either .state or .status
void Controller::_component_changed(string yml_path, string new_status)
{
    string last_part;
    if (get_last_path(yml_path, last_part))
    {
        cerr << __func__ << " component status string doesn't contain a trailing field" 
             << endl;
        return;
    }

    size_t idx = yml_path.find(".status");
    string component_name = yml_path.substr(0, idx);
    if (component_instances.find(component_name) == component_instances.end())
    {
        cerr << __func__ << " unknown component:" << component_name << endl;
        return;
    }
    // ThreadLock<Protected<map<string, string> > > l(component_status);
    component_status.lock();
    component_status[component_name] = new_status;
    component_status.unlock();
    state_condition.signal();
}

bool Controller::_emit_initialize()
{
    return fsm.handle_event("initialize");
}

bool Controller::_emit_stand_down()
{
    return fsm.handle_event("stand_down");
}

bool Controller::_emit_start()
{
    return fsm.handle_event("start");
}

bool Controller::_emit_stop()
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
///
    return false;
}

void Controller::add_factory_method(string name, Component *(*func)(string))
{
    factory_methods[name] = func;
}    



