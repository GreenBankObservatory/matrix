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

#ifndef Component_h
#define Component_h
#include <string>
#include <yaml-cpp/yaml.h>
#include "FiniteStateMachine.h"
#include <tsemfifo.h>
#include <Thread.h>
#include "Keymaster.h"

class ComponentException : public MatrixException
{
public:
    ComponentException(std::string msg) : 
        MatrixException(msg, "Component exception") {}
};


/// This class defines the basic interface between the Controller, Keymaster,
/// and inter-component dataflow setup.
///
/// A derived Component should have (by convention) a static factory method
/// which takes a std::string, and returns a new instance of the component
/// as a Component pointer.
/// e.g:
///       `static Component * MyComponent::factory(std::string, keymaster);`
/// 
/// In the constructor, the Component should contact the Keymaster and
/// register the required keys 'my_instance_name.status', and report a 
/// value of 'created'. Likewise the Component should register and subscribe to
/// the key ,my_instance_name.command. 
/// These two elements form the basis of coordination with the Controller 
/// and the other Components.
/// 
/// 

class Component
{
public:
    Component(std::string myname, std::string keymaster_url);
    virtual ~Component();
    
    /// Initiate a publish/subscribe connection to the keymaster to exchange
    /// commands and status. Configuration information is obtained via the
    /// get/set mechanism.
    void contact_keymaster(std::string url);
    
    /// The base class method just initializes the basic state transitions.
    /// Derived Components may add additional states, events, actions and predicates.
    void initialize_fsm();
    
    /// A new Controller command has arrived. Process it.
    bool process_command(std::string);
    
    ///  Return the current Component state.
    std::string get_state();
    
    /// Send a state change to the keymaster.
    bool report_state(std::string);
    
    /// Make data connections based on the current configuration. Normally
    /// occurs in the Standby to Ready state.
    bool create_data_connections();
    
    /// tear down data connections.
    bool close_data_connections();
    
    /// the keymaster has detected a command change, handle it
    void command_changed(std::string key, YAML::Node n);
    
    /// the internal state has changed, report it to the keymaster
    bool state_changed();
    
    /// A service loop which waits for commands from the controller
    void server_loop();
    
    /// Shutdown the component and any threads it created
    void terminate();
    
    /// handle leaving a state
    bool handle_leaving_state();
    
    /// handle entering a state
    bool handle_entering_state();

    /// callback for the Created to Standby state transition
    bool do_initialize();
            
    /// callback for the Standby to Ready state transition
    bool do_ready();
    
    /// callback for the Ready to Running state transition
    bool do_start();
    
    /// callback for the Running to Ready state transition
    bool do_stop();
    
    /// callback for the Ready to Standby state transition
    bool do_standby();
       
protected:
    // virtual implementations of the public interface
    virtual void _contact_keymaster(std::string url);
    
    /// The base class method just initializes the basic state transitions.
    /// Derived Components may add additional states, events, actions and predicates.
    virtual void _initialize_fsm();
    
    /// A new Controller command has arrived. Process it.
    virtual bool _process_command(std::string);
    
    ///  Return the current Component state.
    virtual std::string _get_state();
    
    /// Send a state change to the keymaster.
    virtual bool _report_state(std::string);
    
    /// Make data connections based on the current configuration. Normally
    /// occurs in the Standby to Ready state.
    virtual bool _create_data_connections();
    
    /// tare down data connections.
    virtual bool _close_data_connections();
    
    virtual void _command_changed(std::string key, YAML::Node n);
    
    virtual bool _state_changed();
    
    virtual void _server_loop();
    
    virtual void _terminate();

    /// handle leaving a state
    virtual bool _handle_leaving_state();
    
    /// handle entering a state
    virtual bool _handle_entering_state();
    
    /// callback for the Created to Standby state transition
    virtual bool _do_initialize();
    
    /// callback for the Standby to Ready state transition
    virtual bool _do_ready();
    
    /// callback for the Ready to Running state transition
    virtual bool _do_start();
    
    /// callback for the Running to Ready state transition
    virtual bool _do_stop();
    
    /// callback for the Ready to Standby state transition
    virtual bool _do_standby();

    
    std::string my_instance_name;
    FSM::FiniteStateMachine fsm;
    std::unique_ptr<Keymaster> keymaster;
    Thread<Component> server_thread;
    tsemfifo<std::string> command_fifo;
    bool done;
    TCondition<bool> thread_started;
};





#endif
