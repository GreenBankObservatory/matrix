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

#ifndef Architect_h
#define Architect_h

#include <string>
#include <memory>
#include "matrix/FiniteStateMachine.h"
#include "matrix/Component.h"
#include <map>
#include <string>
#include <memory>
#include <vector>
#include <tuple>
#include <yaml-cpp/yaml.h>
#include "matrix/TCondition.h"
#include "matrix/Mutex.h"
#include "matrix/ThreadLock.h"
#include "matrix/tsemfifo.h"

//class matrix::Keymaster;
//class matrix::KeymasterServer;

/// Exception type for Architect errors.
namespace matrix
{
    class ArchitectException : public matrix::MatrixException
    {
    public:
        ArchitectException(std::string msg) :
                matrix::MatrixException(msg, "Architect exception")
        {
        }
    };
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
/// - main() creates an instance of a Architect, passing
/// information such as a configuration filename, and a map of
/// Component type names to factory methods.
/// - The Architect creates a Keymaster, and passes on the configuration
/// filename.
/// - The Keymaster reads the configuration file and stores it into a
/// tree-like structure, which reflects the configuration file contents.
/// - The Architect interacts queries the Keymaster for information about
/// the components, and then creates instances of the Components as specified.
/// - As Components are created, they contact the Keymaster and registers themselves 
/// and add entries for its state.
/// - As the Components registered themselves, the Architect subscribes
/// to the state/status entries in the Keymaster.
/// .
///
/// At this point the system is in its initial state.
///
/// Once all Components are in the Standby state, the set_system_mode() should
/// be called to specify which mode in the configuration file is to be used.
///
/// It should be noted that the Architect, as its name implies, acts
/// as the bridge between application control code, and the system
/// state. Applications would typically derive from the Architect to
/// implement additional application-specific control logic.
/// Some methods in the Architect class provide default implementations
/// of commonly used code.
///
/// An example application might look like the following:
/// 
///     Architect::create_keymaster_server(config_file);
///     Architect simple;
///
///     Time::thread_delay(1000000000LL);
///
///     cout << "waiting for all components in standby\n";
///     // Wait 1 sec for initialization
///     bool result = simple.wait_all_in_state("Standby", 1000000);
///     if (!result)
///     {
///         cerr << "initial standby state error\n";
///     }
///     // Now that Components are all in the Standby state, set the system-wide mode.
///     simple.set_system_mode("default");
///
///     // Issue the get_ready event to cause active Components to connect their
///     // DataSink's to DataSource's.
///     simple.ready();
///     result = simple.wait_all_in_state("Ready", 1000000);
///     cout << "all in ready\n";
///
///     simple.start();
///     simple.wait_all_in_state("Running", 1000000);

///
namespace matrix
{
    class Architect : public matrix::Component
    {
    public:
        Architect(std::string name, std::string km_url);

        virtual ~Architect();

        /// Add a component factory constructor for later use in creating
        /// the component instance. The factory signature should be
        /// `       Component * Classname::factory(string type, ComponentFactory);`
        static void add_component_factory(std::string name, matrix::Component::ComponentFactory func);

        /// This reads the connections section of the keymaster database/config file
        /// and for each mode listed, creates a set of instance names of the
        /// active components to be used in a given mode.
        bool configure_component_modes();

        /// Go through configuration, and create instances of the components
        /// This should also cause component threads to be created.
        /// As components are created, they should register themselves
        /// with the keymaster. Note: throws ArchitectException if there
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

        std::shared_ptr<matrix::Component> get_component_by_name(std::string name);

        struct ComponentInfo
        {
            std::shared_ptr<matrix::Component> instance;
            std::string state;
            std::string status;
            bool active;
        };

        static void create_keymaster_server(std::string config_file);

        static void destroy_keymaster_server();

    protected:

        typedef std::map<std::string, matrix::Component::ComponentFactory> ComponentFactoryMap;
        typedef matrix::Protected<std::map<std::string, ComponentInfo> > ComponentMap;
        typedef matrix::Protected<std::map<std::string, std::set<std::string> > > ActiveModeComponentSet;
        typedef std::pair<std::string, std::string> StateReport;

        /// A callback for component state changes.
        void component_state_changed(std::string comp_name, YAML::Node newstate);

        /// A callback for connections state changes. Allows for dynamic
        /// changing of modes.
        void connections_changed(std::string, YAML::Node);

        /// A service thread which examines component states and reports the
        /// aggregated system state.
        void component_state_reporting_loop();

        /// Callback for ".configuration" keyword of controller.
        void system_mode_changed(std::string ymppath, YAML::Node newmode);

        /// This should only be called once. The result will be an initialized
        /// Architect, with all Components created.
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

        // Maps component names to component related data
        ComponentMap components;
        ActiveModeComponentSet active_mode_components;

        // A condition variable for waiting on state updates (TBD)
        std::string current_mode;

        // std::string keymaster_url;
        matrix::TCondition<bool> state_condition;
        matrix::tsemfifo<std::pair<std::string, std::string> > state_fifo;
        matrix::TCondition<bool> state_thread_started;
        matrix::Thread<Architect> state_thread;

        /// A place to store Component factory methods
        /// indexed by Component type, not name.
        static ComponentFactoryMap factory_methods;
        static std::shared_ptr<matrix::KeymasterServer> the_keymaster_server;
    };
};


#endif
