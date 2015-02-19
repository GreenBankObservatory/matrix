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
#include <yaml-cpp/yaml.h>
#include "TCondition.h"

class Controller
{
public:
    Controller(std::string configuration_file);
    virtual ~Controller();
    /// Create the Keymaster and have it read the configuration
    /// file specified.
    bool create_the_keymaster();
    
    /// Add a component factory constructor for later use in creating
    /// the component instance. The factory signature should be
    /// `      Component * Classname::factory(string);`
    void add_factory_method(std::string name, Component *(*)(std::string));
    
    /// Go through configuration, and create instances of the components
    /// This should also cause component threads to be created. 
    /// As components are created, they should register themselves
    /// with the keymaster
    bool create_component_instances();
    
    /// Once components are created, they create status and control
    /// entries in the keymaster. This method connects/subscribes
    /// to the status entries and ??? to the command keys.
    bool subscribe_to_components();
    
    /// Issue the initialize event to the Controller FSM.
    bool initialize();
    /// The opposite of the initialize event.
    bool stand_down();
    /// Issue the start event
    bool start();
    // Issue the stop event
    bool stop();
    /// Attempt to shutdown the system (optional)
    bool exit_system();
    /// Issue an arbitrary user-defined event to the FSM
    bool send_event(std::string event);
    /// Wait until all components report the specified state.
    /// A timeout of zero is a simple poll, returning immediately
    bool wait_for_all(std::string statename, int timeout = 0);
    
    /// A callback for component state changes. TBD
    void component_state_changed(std::string comp_name, std::string newstate);
    
    /// check that all component states are in the state specified.
    virtual bool check_all_in_state(std::string state);
    
protected:
    /// Application specific implementations Are all necessary??
    virtual bool _create_the_keymaster();
    virtual bool _create_component_instances();
    virtual bool _subscribe_to_components();
    virtual bool _initialize();
    virtual bool _stand_down();
    virtual bool _start();
    virtual bool _stop();
    virtual bool _exit_system();
    virtual bool _send_event(std::string event);

    
    /// A place to store Component factory methods
    /// indexed by Component type, not name.
    std::map<std::string, Component *(*)(std::string)> factory_methods;
    std::map<std::string, std::shared_ptr<Component> >component_instances;
    std::vector<std::string> component_states;
    FSM::FiniteStateMachine fsm;
    YAML::Node key_root;
    TCondition<bool> state_condition;
};



#endif
