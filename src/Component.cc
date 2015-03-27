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
Component::Component(string myname, shared_ptr<Keymaster> k) :
    my_instance_name(myname),
    keymaster(k)
{
    // perform other user-defined initializations in derived class
    cout << "components." + myname + ".state" << endl;
    keymaster->put("components." + myname + ".state", "Created", true);
}

Component::~Component()
{

}

void Component::contact_keymaster()
{
    // contact the keymaster
    // create the xxx.status keyword, and set it to 'Created'
    // create the xxx.command keyword and subscribe to it.
}

/// Fill-in a state machine with the basic states and events
void Component::initialize_fsm()
{
    //          current state:     on event:    next state:
    fsm.addTransition("Created", "register",   "Standby");
    fsm.addTransition("Standby", "initialize", "Ready");
    fsm.addTransition("Ready",   "start",      "Running");
    fsm.addTransition("Running", "stop",       "Ready");
    fsm.addTransition("Running", "error",       "Ready");
    fsm.addTransition("Ready",   "stand_down", "Standby");
    fsm.setInitialState("Created");
    fsm.run_consistency_check();
}

// callback for command keyword changes (TBD)
bool Component::process_command(string cmd)
{
    return fsm.handle_event(cmd);
}

string Component::get_state()
{
    return fsm.getState();
}
