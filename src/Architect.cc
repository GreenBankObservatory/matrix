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
#include "matrix/Architect.h"
#include <yaml-cpp/yaml.h>
#include "matrix/ThreadLock.h"
#include "matrix/Keymaster.h"
#include "matrix/yaml_util.h"
#include "matrix/Time.h"

using namespace std;
using namespace Time;
using namespace matrix;

#define dbprintf if(verbose) printf

// Functor to check component states. Could do this differently if we had find_not_if (C++11)
struct NotInState
{
    NotInState(std::string s) : compare_state(s) {}
    // return true if states are not equal
    bool operator()(std::pair<const std::string, Architect::ComponentInfo> &p)
    {
        if (p.second.active)
        {
            return p.second.state != compare_state;
        }
        return false;
    }

    std::string compare_state;
};

static map<int, string> state_enums =
{
    {1, string("Created")},
    {2, string("Standby")},
    {3, string("Ready")},
    {4, string("Running")}
};

static int state_2_enum(std::string s)
{
    for (auto i = state_enums.begin(); i!= state_enums.end(); ++i)
    {
        if (i->second == s)
        {
            return i->first;
        }
    }
    return 0;
}

static bool state_compare(std::pair<const std::string, Architect::ComponentInfo> &a,
                          std::pair<const std::string, Architect::ComponentInfo> &b)
{
    return (state_2_enum(a.second.state) < state_2_enum(b.second.state));
}

namespace matrix
{
    shared_ptr <KeymasterServer>     Architect::the_keymaster_server;
    Architect::ComponentFactoryMap Architect::factory_methods;

//Static method
    void Architect::create_keymaster_server(std::string config_file)
    {
        the_keymaster_server.reset(new KeymasterServer(config_file));
        the_keymaster_server->run();
    }

    void Architect::destroy_keymaster_server()
    {
        the_keymaster_server.reset();
    }

    Architect::Architect(string name, string km_url) :
            Component(name, km_url),
            state_condition(false),
            state_fifo(),
            state_thread_started(false),
            state_thread(this, &Architect::component_state_reporting_loop)
    {
        // re-write the base part of the full instance name to
        // be outside of the component directory
        my_full_instance_name = "architect." + my_instance_name;
    }

    Architect::~Architect()
    {
    }

    void Architect::add_component_factory(std::string name, Component::ComponentFactory func)
    {
        Architect::factory_methods[name] = func;
    }

    typedef tuple <string, string, string> ConnectionsKey;

    bool Architect::configure_component_modes()
    {
        // Now search connection info for modes where this component is active
        YAML::Node km_mode = keymaster->get("connections");

        ThreadLock<decltype(active_mode_components)> l(active_mode_components);
        l.lock();
        try
        {
            active_mode_components.clear();

            // for each modeset
            for (YAML::const_iterator md = km_mode.begin(); md != km_mode.end(); ++md)
            {
                // for each connection listed in that mode ...
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
        catch (YAML::Exception &e)
        {
            cerr << e.what() << endl;
            return false;
        }
        return true;
    }

    bool Architect::create_component_instances()
    {
        YAML::Node km_components = keymaster->get("components");

        dbprintf("Architect::_create_component_instances\n");

        for (YAML::const_iterator it = km_components.begin(); it != km_components.end(); ++it)
        {
            string comp_instance_name = it->first.as<string>();
            YAML::Node type = it->second["type"];

            if (!type)
            {
                throw ArchitectException("No type field for component " + type.as<string>());
            }
            else if (factory_methods.find(type.as<string>()) == factory_methods.end())
            {
                throw ArchitectException("No factory for component of type " + type.as<string>());
            }
            else
            {
                ThreadLock<ComponentMap> l(components);

                auto fmethod = factory_methods.find(type.as<string>());
                string root = "components.";
                // subscribe to the components .state key before creating it
                string key = root + comp_instance_name + ".state";

                keymaster->subscribe(key,
                                     new KeymasterMemberCB<Architect>(this,
                                                                      &Architect::component_state_changed));
                // Now do the actual creation
                l.lock();
                components[comp_instance_name].instance = shared_ptr<Component>(
                        (*fmethod->second)(comp_instance_name, keymaster_url));
                // temporarily mark the component as active. It will be reset
                // when the system mode is set.
                components[comp_instance_name].instance->basic_init();
                components[comp_instance_name].active = true;
                l.unlock();
                // component will now be listening to these...
                keymaster->put(root + comp_instance_name + ".command", "do_init");
                keymaster->put(root + comp_instance_name + ".mode", "default");
            }
        }
        return true;
    }

    std::shared_ptr<Component> Architect::get_component_by_name(std::string name)
    {
        ComponentMap::iterator cm = components.find(name);

        if (cm != components.end())
        {
            return cm->second.instance;
        }

        return std::shared_ptr<Component>();
    }

// Verify all components are in the desired state
    bool Architect::check_all_in_state(string statename)
    {
        NotInState not_in_state(statename);
        ThreadLock<decltype(components)> l(components);
        l.lock();
        auto rtn = find_if(components.begin(), components.end(), not_in_state);
        // If we get to the end of the list, all components are in the desired state
        return rtn == components.end();
    }

// Wait for components to reach a desired state with a timeout.
    bool Architect::wait_all_in_state(string statename, int usecs)
    {
        ThreadLock<decltype(state_condition)> l(state_condition);

        Time_t time_to_quit = getUTC() + ((Time_t) usecs) * 1000L;
        l.lock();
        while (!check_all_in_state(statename))
        {
            state_condition.wait_locked_with_timeout(usecs);
            if (getUTC() >= time_to_quit)
            {
                return false;
            }
        }
        return true;
    }

/// Change/set the system mode. This updates the active
/// fields of components which are included in the
    bool Architect::set_system_mode(string mode)
    {
        bool result = false;
        // modes can only be changed when components are in Standby
        if (!check_all_in_state("Standby"))
        {
            cerr << "Not all components are in Standby state!" << endl;
            string root = "components.";
            ThreadLock<ComponentMap> lk(components);
            lk.lock();
            for (auto p = components.begin(); p != components.end(); ++p)
            {
                if (p->second.active == true &&
                    p->second.state != "Standby")
                {
                    cerr << p->first << " is in state " << p->second.state << endl;
                }
            }
            lk.unlock();
            return false;
        }
        current_mode = mode;
        // disable all components for mode change
        string root = "components.";
        ThreadLock<ComponentMap> l(components);
        l.lock();
        for (auto p = components.begin(); p != components.end(); ++p)
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
        for (auto p = components.begin(); p != components.end(); ++p)
        {
            bool active = active_components.find(p->first) != active_components.end();
            p->second.active = active;
            keymaster->put(root + p->first + ".active", active);
            keymaster->put(root + p->first + ".mode", mode);
            result = true;
        }

        return result;
    }


    void Architect::terminate()
    {
        _terminate();
    }

// Architect final methods
    void Architect::system_mode_changed(string /* yml_path */, YAML::Node updated_mode)
    {
        if (!set_system_mode(updated_mode.as<string>()))
        {
            cerr << "Setting system mode to "
            << updated_mode.as<string>() << " failed" << endl;
        }
        keymaster->put(my_full_instance_name + ".active_configuration",
                       current_mode, true);
    }

    void Architect::component_state_changed(string yml_path, YAML::Node new_status)
    {
        _component_state_changed(yml_path, new_status);
    }

    void Architect::connections_changed(string, YAML::Node)
    {
        configure_component_modes();
    }


// Send an event filtered by the components active status.
    bool Architect::send_event(std::string event)
    {
        YAML::Node myevent(event);
        // for each component, if its active in the current mode, then
        // send it the event.
        for (auto p = components.begin(); p != components.end(); ++p)
        {
            if (p->second.active || event == "do_init")
            {
                keymaster->put("components." + p->first + ".command", myevent);
            }
        }
        return true;
    }

    void Architect::component_state_reporting_loop()
    {
        StateReport report;
        Keymaster km(keymaster_url);
        state_thread_started.signal(true);

        while (!done)
        {
            state_fifo.get(report);

            auto p = std::max_element(components.begin(), components.end(), state_compare);
            dbprintf("%s Max state is %s\n", __PRETTY_FUNCTION__,
                     p->second.state.c_str());
            try
            {
                km.put(my_full_instance_name + ".state", p->second.state, true);
            }
            catch (KeymasterException &g)
            {
                cerr << g.what() << endl;
                throw g;
            }
        }

    }

// virtual methods
    bool Architect::_basic_init()
    {
        bool result;

        result = configure_component_modes() &&
                 create_component_instances();
        try
        {
            // perform other user-defined initializations in derived class
            keymaster->put(my_full_instance_name + ".state", fsm.getState(), true);
            keymaster->subscribe(my_full_instance_name + ".command",
                                 new KeymasterMemberCB<Component>(this,
                                                                  &Component::command_changed));
            keymaster->subscribe(my_full_instance_name + ".configuration",
                                 new KeymasterMemberCB<Architect>(this,
                                                                  &Architect::system_mode_changed));
            keymaster->subscribe("connections",
                                 new KeymasterMemberCB<Architect>(
                                         this, &Architect::connections_changed));
        }
        catch (KeymasterException &e)
        {
            cerr << __PRETTY_FUNCTION__ << " exception: " << e.what() << endl;
            return false;
        }


        cmd_thread.start();
        cmd_thread_started.wait(true);
        state_thread.start();
        state_thread_started.wait(true);
        return result;
    }

// A callback called when the keymaster publish's a components state change.
// Not sure about the type of the component arg.
// I am assuming there will be a last path component of either .state or .status
    void Architect::_component_state_changed(string yml_path, YAML::Node new_state)
    {
        size_t p1, p2;

        if ((p1 = yml_path.find_first_of('.')) == string::npos ||
            (p2 = yml_path.find_last_of('.')) == string::npos)
        {
            cerr << "Bad state string from keymaster:" << yml_path << endl;
            return;
        }

        string component_name = yml_path.substr(p1 + 1, p2 - p1 - 1);

        ThreadLock<ComponentMap> l(components);
        l.lock();
        if (components.find(component_name) == components.end())
        {
            cerr << __classmethod__ << " unknown component:"
            << component_name << " with state " << new_state
            << endl;
            cerr << "known components are:" << endl;
            for (auto i = components.begin(); i != components.end(); ++i)
            {
                cerr << i->first << endl;
            }
            cerr << "end of list" << endl;
            return;
        }
        l.unlock();
        dbprintf("%s component:%s state now %s\n",
                 __PRETTY_FUNCTION__, component_name.c_str(),
                 new_state.as<string>().c_str());

        auto p = make_pair(component_name, new_state.as<string>());
        state_fifo.put(p);

        components[component_name].state = new_state.as<string>();
        state_condition.signal();
    }


    string Architect::_get_state()
    {
        return Component::_get_state();
    }

// These are programmitic hooks into the Architect.
// They are equivalent to setting the .command field of the controller
// via a keymaster.
    bool Architect::_initialize()
    {
        try
        {
            keymaster->put(my_full_instance_name + ".command", "do_init", true);
        }
        catch (KeymasterException &e)
        {
            cerr << e.what() << endl;
            return false;
        }
        return true;
    }

    bool Architect::_ready()
    {
        try
        {
            keymaster->put(my_full_instance_name + ".command", "get_ready", true);
        }
        catch (KeymasterException &e)
        {
            cerr << e.what() << endl;
            return false;
        }
        return true;
    }

    bool Architect::_standby()
    {
        try
        {
            keymaster->put(my_full_instance_name + ".command", "do_standby", true);
        }
        catch (KeymasterException &e)
        {
            cerr << e.what() << endl;
            return false;
        }
        return true;
    }

    bool Architect::_start()
    {
        try
        {
            keymaster->put(my_full_instance_name + ".command", "start", true);
        }
        catch (KeymasterException &e)
        {
            cerr << e.what() << endl;
            return false;
        }
        return true;
    }

    bool Architect::_stop()
    {
        try
        {
            keymaster->put(my_full_instance_name + ".command", "stop", true);
        }
        catch (KeymasterException &e)
        {
            cerr << e.what() << endl;
            return false;
        }
        return true;
    }

    bool Architect::_process_command(std::string cmd)
    {
        dbprintf("%s cmd=%s\n", __PRETTY_FUNCTION__, cmd.c_str());
        send_event(cmd);
        return Component::_process_command(cmd);
    }

    void Architect::_terminate()
    {
        keymaster.reset();
        for (auto i = components.begin(); i != components.end(); ++i)
        {
            i->second.instance->terminate();
        }

        if (cmd_thread.running())
        {
            cmd_thread.stop();
        }

        if (state_thread.running())
        {
            state_thread.stop();
        }
        done = true;
        command_fifo.release();
        state_fifo.release();

    }

}; //namespace matrix



