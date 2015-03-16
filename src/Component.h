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
#include "FiniteStateMachine.h"

/// This class defines the basic interface between the Controller, Keymaster,
/// and inter-component dataflow setup.
///
/// A derived Component should have (by convention) a static factory method
/// which takes a std::string, and returns a new instance of the component
/// as a Component pointer.
/// e.g:
///       `static Component * MyComponent::factory(std::string);`
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
    Component(std::string myname);
    virtual ~Component();
    
    /// Initiate a publish/subscribe connection to the keymaster to exchange
    /// commands and status. Configuration information is obtained via the
    /// get/set mechanism.
    void contact_keymaster();
    
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
    
    /// tare down data connections.
    bool close_data_connections();
   
protected:
    std::string my_instance_name;
    FSM::FiniteStateMachine fsm;
};





#endif
