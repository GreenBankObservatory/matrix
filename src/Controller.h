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
/// It should be noted that the Controller, as its name implies, acts
/// as the bridge between application control code, and the system
/// state. Applications would typically derive from the Controller to
/// implement additional application-specific control logic.
/// Some methods in the Controller class provide default implementations
/// of commonly used code.
///
class Component;
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

class ControllerException : public MatrixException
{
public:
    ControllerException(std::string msg) : 
        MatrixException(msg, "Controller exception") {}
};


typedef  Component *(*ComponentFactory)(std::string, std::string keymaster_url) ;
typedef  std::map<std::string, ComponentFactory> ComponentFactoryMap;
struct ComponentInfo
{
    std::shared_ptr<Component> instance;   
    std::string state;
    std::string status;

    bool active;
};
typedef Protected<std::map<std::string, ComponentInfo> > ComponentMap;
typedef Protected<std::map<std::string, std::set<std::string> > > ActiveModeComponentSet;


class Controller
{
public:
    Controller(std::string configuration_file);
    virtual ~Controller();
    
    /// An initialize routine which creates a keymaster, initializes an
    /// fsm, and creates/subscribes to components.
    bool initialize();
    
    /// Set a specific mode. If multiple modes are not defined, the mode
    /// name 'default' is used.
    bool set_system_mode(std::string mode="default");
    
    /// Create the Keymaster and have it read the configuration
    /// file specified.
    bool create_the_keymaster();
    
    /// Add a component factory constructor for later use in creating
    /// the component instance. The factory signature should be
    /// `      Component * Classname::factory(string);`
    void add_factory_method(std::string name, ComponentFactory func);
    
    /// Go through configuration, and create instances of the components
    /// This should also cause component threads to be created. 
    /// As components are created, they should register themselves
    /// with the keymaster
    bool create_component_instances();
    
    /// This reads the connections section of the keymaster database/config file
    /// and for each mode listed, creates a set of active components to be used
    /// in that mode.
    bool configure_component_modes();
    
    /// Issue the initialize event to the Controller FSM. This should
    /// only be called once. The result will be all components in Standby.
    bool do_initialize();
    
    /// The opposite of the get_ready event. This will transition
    /// active components from the Ready to the Standby state.
    bool do_standby();
    
    /// Issue 'do_ready' event to active components. This will transition
    /// active components from the Standby to the Ready state.
    bool do_ready();
    
    /// Issue the start event to active components. This will transition
    /// active components from the Ready to the Running state.
    bool do_start();
    
    // Issue the stop event to active components. This will transition
    /// active components from the Running to the Ready state.
    bool do_stop();
    
    /// Attempt to shutdown the system (optional)
    bool exit_system();
    
    /// Issue an arbitrary user-defined event to the FSM
    bool send_event(std::string event);
    
    /// A callback for component state changes.
    void component_state_changed(std::string comp_name, YAML::Node newstate);
    
    /// A service thread for receiving state changes
    void service_loop();
    
    /// check that all component states are in the state specified.
    virtual bool check_all_in_state(std::string state);
    
    /// wait until component states are all in the state specified.
    virtual bool wait_all_in_state(std::string statename, int timeout);  
    
    /// shutdown the controller and its components
    void terminate();
    
      
    
protected:
    /// Application specific implementations (or default implementations)
    virtual bool _initialize();
    virtual bool _set_system_mode(std::string);
    virtual bool _create_the_keymaster();
    virtual bool _create_component_instances();
    virtual bool _do_initialize();
    virtual bool _do_ready();
    virtual bool _do_standby();
    virtual bool _do_start();
    virtual bool _do_stop();
    virtual bool _exit_system();
    virtual bool _send_event(std::string event);
    virtual void _component_state_changed(std::string yml_path, YAML::Node new_state);
    virtual bool _configure_component_modes();
    virtual void _service_loop();
    virtual void _terminate();
    
    /// A place to store Component factory methods
    /// indexed by Component type, not name.
    ComponentFactoryMap factory_methods;
    // Maps component names to component related data
    ComponentMap components;
    ActiveModeComponentSet active_mode_components;
    Protected<FSM::FiniteStateMachine> fsm;
    // YAML::Node key_root;
    TCondition<bool> state_condition;
    std::string _conf_file;
    std::unique_ptr<KeymasterServer> km_server;
    std::string current_mode;
    
    // keymaster client
    typedef std::pair<std::string, std::string> StateReport;
    std::unique_ptr<Keymaster> keymaster;
    std::string keymaster_url;
    tsemfifo<StateReport> state_report_fifo;
    bool done;
    Thread<Controller> state_thread;
    TCondition<bool> thread_started;
};



#endif
