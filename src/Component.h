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
#include <tuple>
#include <yaml-cpp/yaml.h>
#include "matrix/FiniteStateMachine.h"
#include <matrix/tsemfifo.h>
#include <matrix/Thread.h>
#include "matrix/matrix_util.h"
#include "matrix/Keymaster.h"

class ComponentException : public MatrixException
{
public:
    ComponentException(std::string msg) :
        MatrixException(msg, "Component exception") {}
};


/// This class defines the basic interface between the Architect, Keymaster,
/// and inter-component dataflow setup.
///
/// A derived Component should have a static factory method
/// which takes two strings: the instance name for the component,
/// and the Keymaster url. The factory then returns a new instance of the
/// Component, returning it as a Component pointer.
/// e.g:
///       `static Component * MyComponent::factory(string name, string km_url);`
///
/// Somewhere in the code, prior to creating components, the factory method
/// should be registered with the Architect, using
/// Architect::add_component_factory().
///
/// In the constructor, the Component should contact the Keymaster and
/// register the required keys 'my_instance_name.state', and report a
/// value of 'Created'. Likewise the Component should register and subscribe to
/// the key 'my_instance_name.command', to listen for commands from the Architect.
/// These keys form the basis of coordination with the Architect
/// and Components.
///
///

class Component
{
public:

    // The signature of Component factory methods, and the map which contains them.
    typedef Component *(*ComponentFactory)(std::string, std::string keymaster_url);

    virtual ~Component();

    /// Perform basic initialization on the Component
    bool basic_init();

    /// tear down data connections.
    bool close_data_connections();

    /// the keymaster has detected a command change, handle it
    void command_changed(std::string key, YAML::Node n);

    /// A service loop which waits for commands from the controller
    void command_loop();

    /// Make data connections based on the current configuration. Normally
    /// occurs in the Standby to Ready state.
    bool create_data_connections();

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

    /// callback for handling an error while in the running state
    bool do_runtime_error();

    ///  Return the current Component state.
    std::string get_state();

    /// Callbacks to handle leaving a state
    bool handle_leaving_state();

    /// Callbacks to handle entering a state
    bool handle_entering_state();

    /// Sends the init event to all Components, placing them all in the Standby
    /// state. This should be called prior to setting the system mode with
    /// set_system_mode().
    bool initialize();

    /// The base class method just initializes the basic state transitions.
    /// Derived Components may add additional states, events, actions and predicates.
    void initialize_fsm();

    /// A new Architect command has arrived. Process it.
    bool process_command(std::string);

    /// Send a state change to the keymaster.
    bool report_state(std::string);

    /// the internal state has changed, report it to the keymaster
    bool state_changed();

    /// The opposite of the get_ready event. This will transition
    /// active components from the Ready to the Standby state.
    bool standby();

    /// Issue 'do_ready' event to active components. This will transition
    /// active components from the Standby to the Ready state.
    bool ready();

    /// Issue the start event to active components. This will transition
    /// active components from the Ready to the Running state.
    bool start();

    /// Issue the stop event to active components. This will transition
    /// active components from the Running to the Ready state.
    bool stop();

    /// Shutdown the component and any threads it created
    void terminate();

protected:

    typedef std::map<std::tuple<std::string,std::string,std::string>, 
                     std::tuple<std::string,std::string,std::string> >  ConnectionMap;
    typedef std::tuple<std::string,std::string,std::string> ConnectionKey;
    
    /// Query for a connection
    bool find_data_connection(ConnectionKey &);
    bool parse_data_connections();
    template<typename J>
    bool connect_sink(J &sink, std::string sinkname);


    void mode_changed(std::string path, YAML::Node n);

    /// The protected constructor, only available from the factory or derived classes
    Component(std::string myname, std::string keymaster_url);

    /// Perform some basic initializtion on the Component.
    virtual bool _basic_init();

    /// tear down data connections.
    virtual bool _close_data_connections();

    virtual void _command_changed(std::string key, YAML::Node n);

    virtual void _command_loop();

    // virtual implementations of the public interface
    virtual bool _contact_keymaster(std::string url);

    /// Make data connections based on the current configuration. Normally
    /// occurs in the Standby to Ready state.
    virtual bool _create_data_connections();

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

    virtual bool _do_runtime_error();

    ///  Return the current Component state.
    virtual std::string _get_state();

    /// handle leaving a state
    virtual bool _handle_leaving_state();

    /// handle entering a state
    virtual bool _handle_entering_state();

    /// Specific implementations of the command methods
    /// Initialize and enter the standby state
    virtual bool _initialize();

    /// The base class method just initializes the basic state transitions.
    /// Derived Components may add additional states, events, actions and predicates.
    virtual void _initialize_fsm();

    /// A new Architect command has arrived. Process it.
    virtual bool _process_command(std::string);

    /// Create DataSink connections and enter the Ready state
    virtual bool _ready();

    /// Close DataSink connections and enter the Standby state
    virtual bool _standby();

    /// From the Ready state, prepare to run and enter the Running state.
    virtual bool _start();

    /// From the Running state, shutdown and enter the Ready state.
    virtual bool _stop();

    /// Send a state change to the keymaster.
    virtual bool _report_state(std::string);

    virtual bool _state_changed();

    virtual void _terminate();

    // Members
    std::string keymaster_url;
    std::string my_instance_name;   /// <== The component's short name
    std::string my_full_instance_name; /// <== The full YAML path for the component
    Protected<FSM::FiniteStateMachine<std::string>> fsm;
    std::shared_ptr<Keymaster> keymaster;
    /// A thingy which has all the connection info for the current mode.
    /// Maps a key of <mode,component,sink> to the corresponding <component,source,transport>
    ConnectionMap connections;
    std::string current_mode; 
    bool done;
    Thread<Component> cmd_thread;
    tsemfifo<std::string> command_fifo;
    TCondition<bool> cmd_thread_started;
    bool verbose; /// <== Controls debug print outs.
};


template <typename T>
bool Component::connect_sink(T &sink, std::string sinkname)
{
    ConnectionKey q(current_mode, my_instance_name, sinkname);
    if (find_data_connection(q))
    {
        sink.connect(std::get<0>(q), std::get<1>(q), std::get<2>(q));
    }
    return true;
}    



#endif
