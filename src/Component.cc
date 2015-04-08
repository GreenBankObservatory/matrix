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

#include "Component.h"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include "Keymaster.h"

using namespace std;
using namespace YAML;
using namespace FSM;


/// The arg 'myname' is the so called instance name from the
/// configuration file.
Component::Component(string myname, string keymaster_url) :
    my_instance_name(myname),
    keymaster(),
    server_thread(this, &Component::server_loop),
    command_fifo(),
    done(false),
    thread_started(false)
{
    initialize_fsm(); 
    contact_keymaster(keymaster_url);   
}

Component::~Component()
{

}

void Component::contact_keymaster(string keymaster_url)
{
    _contact_keymaster(keymaster_url);
}

/// Fill-in a state machine with the basic states and events
void Component::initialize_fsm()
{
    _initialize_fsm();
}

// callback for command keyword changes (TBD)
bool Component::process_command(string cmd)
{
    return _process_command(cmd);
}

string Component::get_state()
{
    return _get_state();
}

bool Component::report_state(string state)
{
    return _report_state(state);
}

bool Component::create_data_connections()
{
    return _create_data_connections();
}

bool Component::close_data_connections()
{
    return _close_data_connections();
}

void Component::command_changed(string key, YAML::Node n)
{
    _command_changed(key, n);
}

bool Component::state_changed()
{
    return _state_changed();
}

/// callback for the Created to Standby state transition
bool Component::do_initialize()
{
    return _do_initialize();
}

/// callback for the Standby to Ready state transition
bool Component::do_ready()
{
    return _do_ready();
}

/// callback for the Ready to Running state transition
bool Component::do_start()
{
    return _do_start();
}

/// callback for the Running to Ready state transition
bool Component::do_stop()
{
    return _do_stop();
}

/// callback for the Ready to Standby state transition
bool Component::do_standby()
{
    return _do_standby();
}

void Component::server_loop()
{
    _server_loop();
}

void Component::terminate()
{
    _terminate();
}

void Component::_server_loop()
{
    thread_started.signal(true);
    string command;
    while (!done)
    {
        command_fifo.get(command);
        // cout << "Component:" << my_instance_name << " command now " << command << endl;
        process_command(command);
    }
}


// virtual private implementations of the public interface
void Component::_contact_keymaster(string keymaster_url)
{
    keymaster.reset( new Keymaster(keymaster_url) );
    // perform other user-defined initializations in derived class  
    keymaster->put(my_instance_name + ".state", fsm.getState(), true);
    // keymaster->put(my_instance_name + ".command", "register", true);
    keymaster->subscribe(my_instance_name + ".command", 
                         new KeymasterMemberCB<Component>(this, 
                         &Component::command_changed) );

    server_thread.start();
    thread_started.wait(true);
}
    
// Setup the states for the default state machine. Attach callbacks
// such that when the state changes, the change is reported to the
// keymaster for publishing.
void Component::_initialize_fsm()
{
    //          current state:     on event:    next state:
    fsm.addTransition("Created", "do_init", "Standby");
    // Commented out to prevent spurious state events before component is fully initialized.
    fsm.addLeaveAction("Created", new Action<Component>(this, &Component::do_initialize) );
    // fsm.addLeaveAction("Created", new Action<Component>(this, &Component::handle_leaving_state) );    
    
    fsm.addTransition("Standby", "get_ready",  "Ready");
    fsm.addEnterAction("Standby", new Action<Component>(this, &Component::do_standby) );
    fsm.addLeaveAction("Standby", new Action<Component>(this, &Component::handle_leaving_state) );
    
    fsm.addTransition("Ready",   "start",      "Running");
    fsm.addEnterAction("Ready", new Action<Component>(this, &Component::do_ready) );
    fsm.addLeaveAction("Ready", new Action<Component>(this, &Component::handle_leaving_state) );
    
    fsm.addTransition("Running", "stop",       "Ready");
    fsm.addEnterAction("Running", new Action<Component>(this, &Component::do_start) );
    fsm.addLeaveAction("Running", new Action<Component>(this, &Component::handle_leaving_state) );
    
    fsm.addTransition("Running", "error",      "Ready");
    
    fsm.addTransition("Ready",   "do_standby", "Standby");
    
    fsm.setInitialState("Created");
    fsm.run_consistency_check();
}

bool Component::handle_leaving_state()
{
    // cout << "Debug:" << my_instance_name << " leaving state " << fsm.getState() << endl;
    return _handle_leaving_state();
}

bool Component::handle_entering_state()
{
    // cout << "Debug:" << my_instance_name << " entering state " << fsm.getState() << endl;
    return _handle_entering_state();
}

bool Component::_handle_leaving_state()
{
    
}
bool Component::_handle_entering_state()
{
    // propagate the state change to the keymaster...
    state_changed();
    return true;
}
 
/// A new Controller command has arrived. Process it.
bool Component::_process_command(std::string cmd)
{
    // cout << "Component::process_command command now " << cmd << endl;
    fsm.handle_event(cmd);
    return true;
}
    
///  Return the current Component state.
std::string Component::_get_state()
{
    return fsm.getState();
}
    
/// Send a state change to the keymaster.
bool Component::_report_state(std::string newstate)
{
    if (keymaster)
        keymaster->put(my_instance_name + ".state", newstate);
    return true;
}

/// Make data connections based on the current configuration. Normally
/// occurs in the Standby to Ready state.
bool Component::_create_data_connections()
{
    return false;
}

/// tare down data connections.
bool Component::_close_data_connections()
{
    return false;
}

void Component::_command_changed(string path, YAML::Node n)
{
    string cmd = n.as<string>();
    command_fifo.put(cmd);
}

bool Component::_state_changed(void)
{
    return report_state( get_state() );
}

void Component::_terminate()
{
    if (server_thread.running())
    {
        server_thread.stop();
        done = true;
    }
    command_fifo.release();
    keymaster.reset();
}

bool Component::_do_initialize()
{
    state_changed();
    return true;
}

bool Component::_do_standby()
{
    state_changed();
    return true;
}

bool Component::_do_ready()
{
    state_changed();
    return true;
}

bool Component::_do_start()
{
    state_changed();
    return true;
}

bool Component::_do_stop()
{
    state_changed();
    return true;
}



