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

#ifndef Controller_h
#define Controller_h

#include <string>
#include <memory>
#include "FiniteStateMachine.h"
#include "Component.h"
#include <map>
#include <string>
#include <memory>
#include <vector>
#include <yaml-cpp/yaml.h>
#include "TCondition.h"
#include "Mutex.h"
#include "ThreadLock.h"
#include "tsemfifo.h"

class Keymaster;
class KeymasterServer;

/// Exception type for Controller errors.
class ControllerException : public MatrixException
{
public:
    ControllerException(std::string msg) :
        MatrixException(msg, "Controller exception") {}
};


///
/// This class acts as an interface/default implementation of a Component
/// controller. Its main purpose is to manage the contained Components,
/// providing a coordinated initialization and shutdown of the Components.
/// It also acts as the creator of Components, based on configuration
/// information from the Keymaster.
///
/// So an application might follow the following sequence:
/// - The application is executed, main() is entered.
/// - main() creates an instance of a Controller, passing
/// information such as a configuration filename, and a dictionary
/// of Component type names to factory methods.
/// - The Controller creates a Keymaster, and passes on the configuration
/// filename.
/// - The Keymaster reads the configuration file and stores it into a
/// tree-like structure, which reflects the configuration file contents.
/// - The Controller interacts (reads) the data, and creates instances
/// of Components specified by the config file.
/// - As Components are created, each of them retrieves the list of
///  inter-component connections along with any special Component configuration
///  information.
/// - Each Component registers itself with the Keymaster and adds
/// entries for its state/status.
/// - As the Components registered themselves, the Controller subscribes
/// to the state/status entries in the Keymaster.
/// .
///
/// At this point the system is in its initial state.
///
/// Once all Components are in the Standby state, the set_system_mode() should
/// be called to specify which mode in the configuration file is to be used.
///
/// It should be noted that the Controller, as its name implies, acts
/// as the bridge between application control code, and the system
/// state. Applications would typically derive from the Controller to
/// implement additional application-specific control logic.
/// Some methods in the Controller class provide default implementations
/// of commonly used code.
///
class Controller : public Component
{
public:
    Controller(KeymasterServer *km, std::string name, std::string km_url);
    virtual ~Controller();

    /// Add a component factory constructor for later use in creating
    /// the component instance. The factory signature should be
    /// `       Component * Classname::factory(string type, ComponentFactory);`
    void add_component_factory(std::string name, Component::ComponentFactory func);

    /// This reads the connections section of the keymaster database/config file
    /// and for each mode listed, creates a set of instance names of the
    /// active components to be used in a given mode.
    bool configure_component_modes();

    /// Go through configuration, and create instances of the components
    /// This should also cause component threads to be created.
    /// As components are created, they should register themselves
    /// with the keymaster. Note: throws ControllerException if there
    /// is no factory registered for the requested Component type.
    bool create_component_instances();

    /// Create the Keymaster and have it read the configuration
    /// file specified.
    bool create_the_keymaster();

    /// check that all component states are in the state specified.
    bool check_all_in_state(std::string state);

    /// wait until component states are all in the state specified.
    bool wait_all_in_state(std::string statename, int timeout);

    /// Issue an arbitrary user-defined event to the FSM.
    bool send_event(std::string event);

    /// Set a specific mode. The mode name should be defined in the "connections"
    /// section of the configuration file.
    bool set_system_mode(std::string mode);

    /// shutdown the controller and its components
    void terminate();

    struct ComponentInfo
    {
        std::shared_ptr<Component> instance;
        std::string state;
        std::string status;
        bool active;
    };

protected:

    typedef std::map<std::string, Component::ComponentFactory> ComponentFactoryMap;
    typedef Protected<std::map<std::string, ComponentInfo> > ComponentMap;
    typedef Protected<std::map<std::string, std::set<std::string> > > ActiveModeComponentSet;
    typedef std::pair<std::string, std::string> StateReport;

    /// A callback for component state changes.
    void component_state_changed(std::string comp_name, YAML::Node newstate);

    /// A service thread which examines component states and reports the
    /// aggregated system state.
    void component_state_reporting_loop();

    /// Callback for ".configuration" keyword of controller.
    void system_mode_changed(std::string ymppath, YAML::Node newmode);

    /// This should only be called once. The result will be an initialized
    /// Controller, with all Components created.
    virtual bool _basic_init();

    /// Overridden callback to handle 'child' component state changes.
    virtual void _component_state_changed(std::string yml_path, YAML::Node new_state);

    ///  Return the current Summary state.
    virtual std::string _get_state();

    /// Specific re-implementations of the command methods.
    /// Initialize and enter the standby state
    virtual bool _initialize();

    /// Create DataSink connections and enter the Ready state
    virtual bool _ready();

    /// Close DataSink connections and enter the Standby state
    virtual bool _standby();

    /// From the Ready state, prepare to run and enter the Running state.
    virtual bool _start();

    /// From the Running state, shutdown and enter the Ready state.
    virtual bool _stop();

    /// A new keymaster command has arrived. Process it.
    virtual bool _process_command(std::string cmd);

    /// Overriden from Component. Recursively calls terminate on all component instances.
    virtual void _terminate();

    // Data members:

    /// A place to store Component factory methods
    /// indexed by Component type, not name.
    ComponentFactoryMap factory_methods;

    // Maps component names to component related data
    ComponentMap components;
    ActiveModeComponentSet active_mode_components;

    // A condition variable for waiting on state updates (TBD)

    std::shared_ptr<KeymasterServer> km_server;
    std::string current_mode;

    // std::string keymaster_url;
    TCondition<bool> state_condition;
    tsemfifo<std::pair<std::string, std::string> > state_fifo;
    TCondition<bool> state_thread_started;
    Thread<Controller> state_thread;
};



#endif
