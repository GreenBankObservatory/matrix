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

#include "matrix/Component.h"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <cstdio>
#include "matrix/Keymaster.h"

using namespace std;
using namespace YAML;
using namespace matrix;
using namespace matrix::FSM;

#define dbprintf if(verbose) printf

/// The arg 'myname' is the so called instance name from the
/// configuration file.
namespace matrix
{
    Component::Component(string myname, string km_url) :
            keymaster_url(km_url),
            my_instance_name(myname),
            my_full_instance_name("components." + my_instance_name),
            keymaster(),
            current_mode("none"),
            done(false),
            cmd_thread(this, &Component::command_loop),
            command_fifo(),
            cmd_thread_started(false),
            verbose(false)
    {
        initialize_fsm();
        _contact_keymaster(keymaster_url);
        parse_data_connections();
    }

    Component::~Component()
    {

    }

// Perform initialization.
    bool Component::basic_init()
    {
        return _basic_init();
    }

    bool Component::close_data_connections()
    {
        return _close_data_connections();
    }

    void Component::command_changed(string key, YAML::Node n)
    {
        _command_changed(key, n);
    }

    void Component::command_loop()
    {
        _command_loop();
    }

    bool Component::parse_data_connections()
    {
        // Now search connection info for modes where this component is active
        YAML::Node km_mode = keymaster->get("connections");

        try
        {
            // for each modeset
            for (YAML::const_iterator md = km_mode.begin(); md != km_mode.end(); ++md)
            {
                // for each connection listed in that mode ...
                for (YAML::const_iterator conn = md->second.begin(); conn != md->second.end(); ++conn)
                {
                    YAML::Node n = *conn;
                    if (n.size() > 2)
                    {
                        // for some clarity...
                        const string &mode = md->first.as<string>();
                        const string &src_comp = n[0].as<string>();
                        const string &src_name = n[1].as<string>();
                        const string &dst_comp = n[2].as<string>();
                        const string &sink_name = n[3].as<string>();

                        if (dst_comp == my_instance_name)
                        {
                            const string protocol = n.size() == 5 ? n[3].as<string>() : "";

                            ConnectionKey ck(mode,
                                             dst_comp,
                                             sink_name);
                            connections[ck] = ConnectionKey(src_comp, src_name, protocol);
                        }
                    }
                }
            }
        }
        catch (YAML::Exception &e)
        {
            cerr << __PRETTY_FUNCTION__ << " " << e.what() << endl;
            return false;
        }
        return true;
    }

    bool Component::find_data_connection(ConnectionKey &c)
    {
        auto conn = connections.find(c);
        if (conn == connections.end())
        {
            return false;
        }
        else
        {
            c = conn->second;
            return true;
        }
    }

    bool Component::create_data_connections()
    {
        return _create_data_connections();
    }

/// Note that in the following do_ methods, if the returned value
/// is not true, the state change will not occur.
/// Callback for the Created to Standby state transition
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

/// callback for the Ready to Standby state transition.
    bool Component::do_standby()
    {
        return _do_standby();
    }

/// callback for handling an error while in the running state
    bool Component::do_runtime_error()
    {
        return _do_runtime_error();
    }

    string Component::get_state()
    {
        return _get_state();
    }

// announce that a state has been left
    bool Component::handle_leaving_state()
    {
        return _handle_leaving_state();
    }

// announce that a new state has been entered.
    bool Component::handle_entering_state()
    {
        return _handle_entering_state();
    }

// Sends the init event to all Components, placing them all in the Standby
// state. This should be called prior to setting the system mode with
// set_system_mode().
    bool Component::initialize()
    {
        return _initialize();
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

// Issue 'do_ready' event to active components. This will transition
// active components from the Standby to the Ready state.
    bool Component::ready()
    {
        return _ready();
    }

    bool Component::report_state(string state)
    {
        return _report_state(state);
    }


    bool Component::state_changed()
    {
        return _state_changed();
    }


// The opposite of the get_ready event. This will transition
// active components from the Ready to the Standby state.
    bool Component::standby()
    {
        return _standby();
    }

// Issue the start event to active components. This will transition
// active components from the Ready to the Running state.
    bool Component::start()
    {
        return _start();
    }

// Issue the stop event to active components. This will transition
// active components from the Running to the Ready state.
    bool Component::stop()
    {
        return _stop();
    }


    void Component::terminate()
    {
        _terminate();
    }

// Default or Component specific implementations
// of the public interfaces.
/***********************************************************************/

    bool Component::_basic_init()
    {
        cmd_thread.start();
        cmd_thread_started.wait(true);

        try
        {
            // perform other user-defined initializations in derived class

            // Create some keymaster keys that this component will need:
            keymaster->put(my_full_instance_name + ".state", fsm.getState(), true);
            keymaster->put(my_full_instance_name + ".command", "none", true);
            keymaster->put(my_full_instance_name + ".active", false, true);
            keymaster->put(my_full_instance_name + ".mode", "default", true);
            // Subscribe to command and mode. Component will react to these.
            keymaster->subscribe(my_full_instance_name + ".command",
                                 new KeymasterMemberCB<Component>(this,
                                                                  &Component::command_changed));
            keymaster->subscribe(my_full_instance_name + ".mode",
                                 new KeymasterMemberCB<Component>(this,
                                                                  &Component::mode_changed));
        }
        catch (KeymasterException &e)
        {
            cerr << __PRETTY_FUNCTION__ << " exception: " << e.what() << endl;
            return false;
        }

        return true;
    }

/// tear down data connections.
    bool Component::_close_data_connections()
    {
        return false;
    }

    void Component::_command_changed(string path, YAML::Node n)
    {
        dbprintf("Component::_command_changed for %s to %s\n",
                 path.c_str(), n.as<string>().c_str());
        string cmd = n.as<string>();
        command_fifo.put(cmd);
    }

    void Component::mode_changed(string, YAML::Node n)
    {
        current_mode = n.as<string>();
    }

// A dedicated thread which executes commands as they come in via the
// command event fifo.
    void Component::_command_loop()
    {
        cmd_thread_started.signal(true);
        string command;
        while (!done)
        {
            try
            {
                command_fifo.get(command);
            }
            catch (Exception &e)
            {
                cerr << e.what() << endl;
                throw e;
            }

            dbprintf("%s: %s processing command %s\n", __PRETTY_FUNCTION__,
                     my_instance_name.c_str(), command.c_str());
            process_command(command);
        }
    }

// virtual private implementations of the public interface
    bool Component::_contact_keymaster(string keymaster_url)
    {
        // Use the true flag to enable the thread safe mode of keymaster.
        keymaster.reset(new Keymaster(keymaster_url, true));
        return true;
    }

/// Make data connections based on the current configuration. Normally
/// occurs in the Standby to Ready state.
    bool Component::_create_data_connections()
    {
        return false;
    }

    bool Component::_do_initialize()
    {
        return true;
    }

    bool Component::_do_standby()
    {
        return true;
    }

    bool Component::_do_ready()
    {
        return true;
    }

    bool Component::_do_start()
    {
        return true;
    }

    bool Component::_do_stop()
    {
        return true;
    }

    bool Component::_do_runtime_error()
    {
        return true;
    }

///  Return the current Component state.
    std::string Component::_get_state()
    {
        return fsm.getState();
    }

    bool Component::_handle_leaving_state()
    {
        return true;
    }

    bool Component::_handle_entering_state()
    {
        // propagate the state change to the keymaster...
        dbprintf("%s: state changed\n", __PRETTY_FUNCTION__);
        state_changed();
        return true;
    }

    bool Component::_initialize()
    {
        return true;
    }

// Setup the states for the default state machine. Attach callbacks
// such that when the state changes, the change is reported to the
// keymaster for publishing.
    void Component::_initialize_fsm()
    {
        // Setup the default state machine, and install the predicate methods which
        // get called when the respect event is received, and return a boolean
        // indicating whether or not the event handling was successful (i.e
        // whether or not the state change should take place.)
        //
        //          current state:     on event:    next state:  predicate method:
        fsm.addTransition("Created", "do_init", "Standby",
                          new Action<Component>(this, &Component::do_initialize));
        fsm.addTransition("Standby", "get_ready", "Ready",
                          new Action<Component>(this, &Component::do_ready));
        fsm.addTransition("Ready", "start", "Running",
                          new Action<Component>(this, &Component::do_start));
        fsm.addTransition("Running", "stop", "Ready",
                          new Action<Component>(this, &Component::do_stop));
        fsm.addTransition("Running", "error", "Ready",
                          new Action<Component>(this, &Component::do_runtime_error));
        fsm.addTransition("Ready", "do_standby", "Standby",
                          new Action<Component>(this, &Component::do_standby));

        // Now add method callbacks which announce the state changes when a new state is entered.
        fsm.addEnterAction("Ready", new Action<Component>(this, &Component::handle_entering_state));

        fsm.addEnterAction("Running", new Action<Component>(this, &Component::handle_entering_state));

        fsm.addEnterAction("Standby", new Action<Component>(this, &Component::handle_entering_state));

        fsm.addEnterAction("Ready", new Action<Component>(this, &Component::handle_entering_state));

        fsm.setInitialState("Created");
        fsm.run_consistency_check();
    }

/// A new Architect command has arrived. Process it.
    bool Component::_process_command(std::string cmd)
    {
        dbprintf("Component::process_command: %s command now %s\n",
                 my_instance_name.c_str(), cmd.c_str());
        if (!fsm.handle_event(cmd))
        {
            // cerr << "Component FSM "<< my_instance_name << " rejected event "
            //     << cmd << " while in state:" << fsm.getState() << endl;
            // This gets reported by FSM.
        }
        return true;
    }

/// Create DataSink connections and enter the Ready state
    bool Component::_ready()
    {
        return true;
    }

/// Send a state change to the keymaster.
    bool Component::_report_state(std::string newstate)
    {
        dbprintf("%s: reporting new state -->> %s\n", __PRETTY_FUNCTION__, newstate.c_str());
        keymaster->put(my_full_instance_name + ".state", newstate);
        return true;
    }

    bool Component::_state_changed(void)
    {
        dbprintf("%s: reporting state change\n", __PRETTY_FUNCTION__);
        return report_state(get_state());
    }


/// Close DataSink connections and enter the Standby state
    bool Component::_standby()
    {
        return true;
    }

/// From the Ready state, prepare to run and enter the Running state.
    bool Component::_start()
    {
        return true;
    }

/// From the Running state, shutdown and enter the Ready state.
    bool Component::_stop()
    {
        return true;
    }

    void Component::_terminate()
    {
        if (cmd_thread.running())
        {
            cmd_thread.stop();
            done = true;
        }
        command_fifo.release();

        keymaster.reset();
    }

}; // namespace matrix


