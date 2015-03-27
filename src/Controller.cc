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


// Functor to check component states
struct Not_in_state
{
    Not_in_state(string s) : compare_state(s) {}
    // return true if states are not equal
    bool operator()(std::pair<const string, ComponentInfo> &p)
    {
        if (p.second.active)
        {
            return p.second.state != compare_state;
        }
        return false;
    }

    string compare_state;
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
    _conf_file(config_file), 
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
bool Controller::initialize()
{
    return _initialize();
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

bool Controller::do_initialize()
{
    return _do_initialize();
}

bool Controller::do_standby()
{
    return _do_standby();
}

bool Controller::do_ready()
{
    return _do_ready();
}

bool Controller::do_start()
{
    return _do_start();
}

bool Controller::do_stop()
{
    return _do_stop();
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
    Not_in_state not_in_state(statename);
    ThreadLock<decltype(components)> l(components);
    auto rtn = find_if(components.begin(), components.end(), not_in_state);
    // If we get to the end of the list, all components are in the desired state
    return rtn == components.end();
}

// 
bool Controller::wait_all_in_state(string statename, int usecs)
{
    ThreadLock<decltype(state_condition)> l(state_condition);
    timespec curtime;
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


bool Controller::_initialize()
{
    fsm.addTransition("Created",  "do_init",         "Standby");
    fsm.addTransition("Standby",  "get_ready",       "Ready");
    fsm.addTransition("Ready",    "start",           "Running");
    fsm.addTransition("Running",  "error",           "Ready");    
    fsm.addTransition("Running",  "stop",            "Ready");
    fsm.addTransition("Ready",    "do_standby",      "Standby");    
    fsm.setInitialState("Created");

    bool result = true;
    result = create_the_keymaster() &&
             _configure_component_modes() &&
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
    ThreadLock<ComponentMap> l(components);
    for (auto p=components.begin(); p!=components.end(); ++p)
    {
        p->second.active = false;
        keymaster->put(p->first + ".active", false);
    }
    l.unlock();
    
    auto modeset = active_mode_components.find(mode);
    if (modeset == active_mode_components.end())
    {
        // no components or no entry for this mode. we are done.
        return false;
    }
        
    auto active_components = modeset->second;
    // All components are in Standby, so we just need to set/reset active flag
    l.lock();
    for (auto p=components.begin(); p!=components.end(); ++p)
    {
        bool active = active_components.find(p->first) != active_components.end();
        p->second.active = active;
        keymaster->put(p->first + ".active", active);
        result = true;
    }

    return result;
}

bool Controller::_create_the_keymaster()
{
    km_server.reset( new KeymasterServer(_conf_file) );
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
        string comp_instance_name = "components." + it->first.as<string>();
        cout << "\t" << it->first << endl;
        YAML::Node type = it->second["type"];
        
        if (!type)
        {
            cerr << __classmethod__ << " No type field for component " 
                 << comp_instance_name << endl;
            return false;
        }
        else if (factory_methods.find(type.as<string>()) == factory_methods.end())
        {
            cerr << "No factory for component of type " << type 
                 << " found in factory list" << endl;
            return false;
        }     
        else
        {        
            auto fmethod = factory_methods.find(type.as<string>());
            // subscribe to the components .state key before creating it
            string key = comp_instance_name + ".state";
            keymaster->subscribe(key, 
                                 new KeymasterMemberCB<Controller>(this, 
                                     &Controller::component_state_changed));
            // create a .command key for the component to listen to
            keymaster->put(comp_instance_name + ".command", "none", true);
            keymaster->put(comp_instance_name + ".active", false,   true);
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
    // connection: <--- map
    //    SOMEMODE: <--- map
    //       - [A,a,B,b,T] <-- vector
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
                        .insert("components." + n[0].as<string>());
                }
                if (n.size() > 2)
                {
                    active_mode_components[md->first.as<string>()]
                        .insert("components." + n[2].as<string>());                    
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
    size_t pos = yml_path.find_last_of('.');
    string component_name = yml_path.substr(0,pos);
    ThreadLock<ComponentMap> l(components);
    if (components.find(component_name) == components.end())
    {
        cerr  << __classmethod__ << " unknown component:" 
              << component_name << endl;
        return;
    }
    
    auto p = make_pair(yml_path, new_state.as<string>());
    state_report_fifo.put(p); 
    
    components[component_name].state = new_state.as<string>();
    components.unlock();
    state_condition.signal();
}

bool Controller::_do_initialize()
{
    return send_event("do_init");
}

bool Controller::_do_standby()
{
    return send_event("do_standby");
}

bool Controller::_do_ready()
{
    return send_event("get_ready");
}

bool Controller::_do_start()
{
    return send_event("start");
}

bool Controller::_do_stop()
{
    return send_event("stop");
}

bool Controller::_exit_system()
{
    cout << __classmethod__ << " - not implemented" << endl;
    return false;
}

void Controller::add_factory_method(std::string name, ComponentFactory func)
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
            keymaster->put(p->first + ".command", myevent);
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
        cout << __classmethod__ << report.first << " is now " << report.second << endl;
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


