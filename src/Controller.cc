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
#include "Keymaster.h"
#include "yaml_util.h"
#include "Time.h"

using namespace std;
using namespace Time;

// Functor to check component states. Could do this differently if we had find_not_if (C++11)
struct NotInState
{
    NotInState(std::string s) : compare_state(s) {}
    // return true if states are not equal
    bool operator()(std::pair<const std::string, Controller::ComponentInfo> &p)
    {
        if (p.second.active)
        {
            return p.second.state != compare_state;
        }
        return false;
    }

    std::string compare_state;
};



string lstrip(const string s)
{
    size_t d = s.find_first_of(".");
    if (d == string::npos)
    {
        return s;
    }
    return s.substr(d+1, string::npos);    
}

Controller::Controller(string config_file) : 
    state_condition(false), 
    conf_file(config_file), 
    keymaster_url("inproc://matrix.keymaster"),
    state_report_fifo(),
    done(false),
    state_thread(this, &Controller::service_loop),
    thread_started(false)
{
}

Controller::~Controller()
{
}

/// Initialize the system. This should be called once after system
/// startup.
bool Controller::basic_init()
{
    return _basic_init();
}

bool Controller::create_the_state_machine()
{
    return _create_the_state_machine();
}

/// Change/set the system mode. This updates the active
/// fields of components which are included in the 
bool Controller::set_system_mode(string mode)
{
    return _set_system_mode(mode);
}

bool Controller::create_the_keymaster()
{
    return _create_the_keymaster();
}

bool Controller::create_component_instances()
{
    return _create_component_instances();
}

bool Controller::configure_component_modes()
{
    return _configure_component_modes();
}

bool Controller::initialize()
{
    return _initialize();
}

bool Controller::standby()
{
    return _standby();
}

bool Controller::ready()
{
    return _ready();
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

bool Controller::check_all_in_state(string statename)
{
    NotInState not_in_state(statename);
    ThreadLock<decltype(components)> l(components);
    auto rtn = find_if(components.begin(), components.end(), not_in_state);
    // If we get to the end of the list, all components are in the desired state
    return rtn == components.end();
}

// 
bool Controller::wait_all_in_state(string statename, int usecs)
{
    ThreadLock<decltype(state_condition)> l(state_condition);
    timespec curtime, to;
    
    Time_t time_to_quit = getUTC() + ((Time_t)usecs)*1000L;    
    while ( !check_all_in_state(statename) )
    {
        state_condition.wait_locked_with_timeout(usecs);
        if (getUTC() >= time_to_quit)
        {
            return false;
        }
    }    
    return true;
}

void Controller::service_loop()
{
    _service_loop();
}

void Controller::terminate()
{
    _terminate();
}

bool Controller::_create_the_state_machine()
{
    fsm.addTransition("Created",  "do_init",         "Standby");
    fsm.addTransition("Standby",  "get_ready",       "Ready");
    fsm.addTransition("Ready",    "start",           "Running");
    fsm.addTransition("Running",  "error",           "Ready");    
    fsm.addTransition("Running",  "stop",            "Ready");
    fsm.addTransition("Ready",    "do_standby",      "Standby");    
    fsm.setInitialState("Created");    
}

bool Controller::_basic_init()
{
    bool result;
    result = create_the_state_machine() &&
             create_the_keymaster() &&
             configure_component_modes() &&
             create_component_instances();
    return result;
}

bool Controller::_set_system_mode(string mode)
{
    bool result = false;
    // modes can only be changed when components are in Standby
    if (!check_all_in_state("Standby"))
    {
        cerr << "Not all components are in Standby state!" << endl;
        return false;
    }
    current_mode = mode;
    // disable all components for mode change
    string root = "components.";
    ThreadLock<ComponentMap> l(components);
    for (auto p=components.begin(); p!=components.end(); ++p)
    {
        p->second.active = false;
        keymaster->put(root + p->first + ".active", false);
    }
    l.unlock();
    
    auto modeset = active_mode_components.find(mode);
    if (modeset == active_mode_components.end())
    {
        // no components or no entry for this mode. we are done.
        // TBD is this really an error???
        return false;
    }
        
    auto active_components = modeset->second;
    // All components are in Standby, so we just need to set/reset active flag
    l.lock();
    for (auto p=components.begin(); p!=components.end(); ++p)
    {
        bool active = active_components.find(p->first) != active_components.end();
        p->second.active = active;
        keymaster->put(root + p->first + ".active", active);
        result = true;
    }

    return result;
}

bool Controller::_create_the_keymaster()
{
    km_server.reset( new KeymasterServer(conf_file) );
    km_server->run();

    keymaster.reset( new Keymaster(keymaster_url) );
    keymaster->put("controller.active", 1, true);
    return true;
}

bool Controller::_create_component_instances()
{
    state_thread.start();
    if (!thread_started.value())
        thread_started.wait(true);

    YAML::Node km_components = keymaster->get("components");
    bool result;
    ThreadLock<ComponentMap> l(components);
    
    for (YAML::const_iterator it = km_components.begin(); it != km_components.end(); ++it)
    {
        string comp_instance_name = it->first.as<string>();
        YAML::Node type = it->second["type"];
        
        if (!type)
        {
            throw ControllerException("No type field for component " + type.as<string>());
        }
        else if (factory_methods.find(type.as<string>()) == factory_methods.end())
        {
            throw ControllerException("No factory for component of type " + type.as<string>());
        }     
        else
        {        
            auto fmethod = factory_methods.find(type.as<string>());
            string root = "components.";
            // subscribe to the components .state key before creating it
            string key = root + comp_instance_name + ".state";
            keymaster->subscribe(key, 
                                 new KeymasterMemberCB<Controller>(this, 
                                     &Controller::component_state_changed));
            // create a .command key for the component to listen to
            keymaster->put(root + comp_instance_name + ".command", "none", true);
            keymaster->put(root + comp_instance_name + ".active", false,   true);
            // Now do the actual creation          
            components[comp_instance_name].instance = shared_ptr<Component>( 
                (*fmethod->second)(comp_instance_name, keymaster_url) );
            // temporarily mark the component as active. It will be reset
            // when the system mode is set.   
            components[comp_instance_name].active = true;
        }
    }
    return true;
}

void Controller::component_state_changed(string yml_path, YAML::Node new_status)
{
    _component_state_changed(yml_path, new_status);
}

bool Controller::_configure_component_modes()
{
    // Now search connection info for modes where this component is active
    YAML::Node km_mode = keymaster->get("connections");

    ThreadLock<decltype(active_mode_components)> l(active_mode_components);
    try
    {
        for (YAML::const_iterator md = km_mode.begin(); md != km_mode.end(); ++md)
        {
            for (YAML::const_iterator conn = md->second.begin(); conn != md->second.end(); ++conn)
            {
                YAML::Node n = *conn;
                if (n.size() > 0)
                {
                    active_mode_components[md->first.as<string>()]
                        .insert(n[0].as<string>());
                }
                if (n.size() > 2)
                {
                    active_mode_components[md->first.as<string>()]
                        .insert(n[2].as<string>());
                }
            }
        }
    } 
    catch (YAML::Exception e)
    {
        cerr << e.what() << endl;
        return false;
    }
    return true;
}


// A callback called when the keymaster publish's a components state change.
// Not sure about the type of the component arg. 
// I am assuming there will be a last path component of either .state or .status
void Controller::_component_state_changed(string yml_path, YAML::Node new_state)
{
    size_t p1, p2;

    if ((p1  = yml_path.find_first_of('.')) == string::npos ||
        (p2  = yml_path.find_last_of('.')) == string::npos)
    {
        cerr << "Bad state string from keymaster:" << yml_path << endl;
        return;
    }

    string component_name = yml_path.substr(p1+1,p2-p1-1);

    ThreadLock<ComponentMap> l(components);
    if (components.find(component_name) == components.end())
    {
        cerr  << __classmethod__ << " unknown component:" 
              << component_name << endl;
        cerr << "known components are:" << endl;
        for (auto i=components.begin(); i!=components.end(); ++i)
        {
            cerr << i->first << endl;
        }
        cerr << "end of list" << endl;
        return;
    }
    
    auto p = make_pair(component_name, new_state.as<string>());
    state_report_fifo.put(p); 
    
    components[component_name].state = new_state.as<string>();
    state_condition.signal();
}

bool Controller::_initialize()
{
    return send_event("do_init");
}

bool Controller::_standby()
{
    return send_event("do_standby");
}

bool Controller::_ready()
{
    return send_event("get_ready");
}

bool Controller::_start()
{
    return send_event("start");
}

bool Controller::_stop()
{
    return send_event("stop");
}

bool Controller::_exit_system()
{
    cout << __classmethod__ << " - not implemented" << endl;
    return false;
}

void Controller::add_component_factory(std::string name, Component::ComponentFactory func)
{
    factory_methods[name] = func;
}    

bool Controller::_send_event(std::string event)
{
    YAML::Node myevent(event);
    // for each component, if its active in the current mode, then
    // send it the event.
    for (auto p = components.begin(); p!=components.end(); ++p)
    {
        if (p->second.active || event == "do_init")
        {
            keymaster->put("components." + p->first + ".command", myevent);
        }
    }
    return true;
}

// net yet sure how this will be used ...
void Controller::_service_loop()
{
    StateReport report;
    thread_started.signal(true);

    while(!done)
    {
        state_report_fifo.get(report);
    }
}

void Controller::_terminate()
{
    keymaster.reset();
    for (auto i = components.begin(); i!= components.end(); ++i)
    {
        i->second.instance->terminate();
    }
    
    if (state_thread.running())
    {
        state_thread.stop();
        done = true;
    }
    state_report_fifo.release();

}


