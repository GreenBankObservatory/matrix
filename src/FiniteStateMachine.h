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

#ifndef FiniteStateMachine_h
#define FiniteStateMachine_h

#include<cstdio>
#include<vector>
#include<map>
#include<memory>
#include<string>

///
/// Documentation for the FiniteStateMachine. This provides a simple yet extensible
/// way to express a State machine.
///
namespace FSM 
{

class ActionBase
{
///
/// A base class to allow derived templated Action objects to be stored in
/// stl containers. User defined predicates and actions must be duck-typed
/// using the signature shown in the do_action method below. For predicates,
/// the return value indicates whether or not the state change should be allowed.
/// For user-defined Actions, the return value is ignored.
///
public:
    virtual bool do_action() { return false; }
};


/// A wrapper template to register callbacks for State change predicates or Actions.
template <typename T>
class Action : public ActionBase
{
public:
    typedef bool (T::*ActionMethod)(void);

    T  *_object;
    ActionMethod _faction;

    Action(T *obj, ActionMethod cb) :
      _object(obj),
      _faction(cb)
    {
    }
    ///
    /// Invoke a call to the user provided callback
    ///
    bool do_action()
    {
        if (_object && _faction)
        {
            return (_object->*_faction)();
        }
        else
        {
            return false;
        }
    }
};

class StateTransition
{
public:

    StateTransition(std::string event,
                    std::string nextstate) :
        event_name(event),
        next_state(nextstate), 
        predicates()
    {
    
    }
    std::string getNextState() { return next_state; }
    std::string getEvent()     { return event_name; }
    bool check_predicates();
    void addPredicate(ActionBase *);
    
protected:
    std::string event_name;
    std::string next_state;
    std::vector<std::shared_ptr<ActionBase> > predicates;
};
                     
                    

/// 
/// State Objects hold a list of possible events and transitions,
/// and Actions to take upon entering or leaving the State.
class State
{
public:
    /// Creates an empty State
    State(std::string statename);
    State();
    /// Causes an event to be processed by the State. If the event
    /// is handled, the return value will be true and nxt_state will
    /// contain the name of the next state. If the return value is false,
    /// the contents of nxt_state is undefined.
    bool handle_event(std::string event, std::string &nxt_state);
    /// Query the state to check if an event is recognized.
    bool is_event_known(std::string);
    /// Add a state to state transition, optionally attaching an Action to be
    /// called if the event causes a state change.
    void addTransition(std::string event, std::string next_state, ActionBase *pred=0);
    /// Add a predicate callback which will be called whenever the named event is
    /// received. The result of the predicate call must be true for the transition to proceed.
    /// Otherwise the event is ignored, and no state change will take place.
    void addEventPredicate(std::string eventname, ActionBase *);
    /// Add a callback when the State is entered.
    void addEnterAction(ActionBase *);
    /// Add a callback when the State is exited
    void addLeaveAction(ActionBase *);
    
    std::string getName() { return state_name; }
    void call_enter_action();
    void call_exit_action();
protected:
    std::map<std::string, StateTransition> transitionmap;
    std::shared_ptr<ActionBase> enterAction, leaveAction; 
    std::string state_name;
};

///
/// 
class FiniteStateMachine
{
///
/// FiniteStateMachine (FSM) Implementation
///
/// Concepts are the following:
/// \li an FSM contains a number of possible states
/// \li each State contains a mapping of input events to possible StateTransitions
/// \li each StateTransition contains a list of user-defined predicates to be tested
/// to decide whether or not the transition should proceed. The predicates are contained
/// within the Action template.
/// \li When a StateTransition causes a change of State, the user-defined enter/exit
/// actions are called to process the State change.
/// 
/// States and Events are denoted as strings.
///
/// Concrete Example:
/// 
/// Consider a FSM model of a PC power supply controlled by a simple pushbutton. 
/// It has three States:
/// \li Off 
/// \li On
/// \li Standby
///
/// Just like on a real PC, pushing the button when the PC is off causes the power to come on.
/// But once on, a quick button press places the PC in a 'hibernating' mode, with the power
/// supply in 'Standby'. A long press while on, does something different -- it causes the
/// supply to turn off all voltage.
///
/// So already we have described some behavior and transitions:
/// \li While in state:  Given the event: Change state to:
/// \li "Off":      "short_press":          "On"
/// \li "Off":      "long_press":          "On"
/// \li "On":       "long_press":          "Off"
/// \li "On":      "short_press":          "Standby"
/// \li "Standby":  "long_press":          "Off"
/// \li "Standby":  "short_press":          "On"
///
/// See unit tests for the implementation of the example above. <BR><BR>

public:
    FiniteStateMachine() : current_state("unknown"),
    prior_state("unknown"),
    initial_state("unknown")
    {}
    /// Cause a new empty state to be created
    void addState(std::string statename);
    /// Define a transition between states. If the States do not exist, they are created
    void addTransition(std::string from_state,
                       std::string event_name,
                       std::string to_state,
                       ActionBase *p = 0);
    /// Register a callback for when the named state is exited
    void addLeaveAction(std::string leaving_state, ActionBase *p);
    /// Register a callback for when the named state is entered
    void addEnterAction(std::string entering_state, ActionBase *p);
    /// Specify the initial state
    void setInitialState(std::string init);    
    
    // These methods are commonly used at runtime to process events:
    /// send an event into the state machine. The return value indicates
    /// whether or not the event caused a state change.
    bool handle_event(std::string event);
        
    /// Returns the name of the current state    
    std::string getState() { return current_state; }
  
protected:
    typedef std::map<std::string, State> Statemap;
    std::string current_state, prior_state, initial_state;
    Statemap states;
};

};
#endif

