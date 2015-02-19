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
    void contact_keymaster();
    void initialize_fsm();
    bool process_command(std::string);
    std::string get_state();
   
protected:
    std::string my_instance_name;
    FSM::FiniteStateMachine fsm;
};





#endif
