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
#include<iostream>
#include<vector>
#include<map>
#include<memory>
#include<string>
#include<algorithm>

///
/// Documentation for the FiniteStateMachine. This provides a simple yet extensible
/// way to express a State machine. Code examples are given in the unit tests.
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
    virtual bool do_action()
    {
        return false;
    }
};


/// A wrapper template to register callbacks for State change predicates or Actions.
/// User callback method signature should match the ActionMethod typedef below.
/// e.g. bool MyClass::mycallback(void)
///
/// Example Use:
/// ------------
///
///       class MyClass { bool mycallback() { cout << "hello" << end; } };
///       Action<MyClass> *p = new Action<MyClass>(&myobj, &MyClass::mycallback);
///
/// Note: The object instance must remain valid for the lifetime of the Action.

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

/// This class encapsulates a state machine 'arc' between two states.
/// A StateTransition may optionally have a set of predicate methods
/// to call, which must all return true in order for the transition
/// to succeed. An optional set of user-defined actions are called
/// when a transition is taken.
template<typename T>
class StateTransition
{
public:

    StateTransition(T event,
                    T nextstate) :
        event_name(event),
        next_state(nextstate),
        predicates()
    {

    }
    T getNextState()
    {
        return next_state;
    }
    T getEvent()
    {
        return event_name;
    }
    bool check_predicates()
    {
        bool result = true;
        for (auto p = predicates.begin(); result && p!=predicates.end(); ++p)
        {
            result = (*p)->do_action() && result;
        }
        return result;
    }

    void addPredicate(ActionBase *predicate, ActionBase *arc_action)
    {
        if (predicate)
        {
            predicates.push_back(std::shared_ptr<ActionBase>(predicate));
        }
        if (arc_action)
        {
            arc_actions.push_back(std::shared_ptr<ActionBase>(arc_action));
        }
    }
    
    bool call_arc_actions()
    {
        for (auto action=arc_actions.begin(); action != arc_actions.end(); ++action)
        {
            (*action)->do_action(); // return value ignored
        }
        return true;
    }

protected:
    T event_name;
    T next_state;
    std::vector<std::shared_ptr<ActionBase> > predicates;
    std::vector<std::shared_ptr<ActionBase> > arc_actions;
};



///
/// State Objects hold a list of possible events and transitions,
/// and Actions to take upon entering or leaving the State.
template<typename T>
class State
{
public:
    /// Creates an empty State
    State(T statename) : transitionmap(), enterAction(), leaveAction(),
        state_name(statename) { }
    State() : transitionmap(), enterAction(), leaveAction(),
        state_name() { }

    /// Causes an event to be processed by the State. If the event
    /// is handled, the return value will be true and nxt_state will
    /// contain the name of the next state. If the return value is false,
    /// the contents of nxt_state is undefined.
    bool handle_event(T event, T &nxt_state)
    {
        auto tr = transitionmap.find(event);
        if (tr == transitionmap.end())
        {
            // dont have any legal transitions for this event
            std::cerr << "FSM: Event:" << event << " unrecognized -- no actions taken"
                      << std::endl;
            return false;
        }
        if (tr->second.check_predicates())
        {
            // predicates check, we are going to follow the transition.
            // call the transition arc action method if it exists
            tr->second.call_arc_actions();
            nxt_state = tr->second.getNextState();
            return true;
        }
        return false; // no state change occurred

    }

    /// Query the state to check if an event is recognized.
    bool is_event_known(T s)
    {
        return (transitionmap.find(s) != transitionmap.end());
    }

    /// Add a state to state transition, optionally attaching an Action to be
    /// called if the event causes a state change.
    void addTransition(T event, T next_state, ActionBase *predicate=0, ActionBase *arc_action=0)
    {
        auto st = transitionmap.find(event);
        if (st == transitionmap.end())
        {
            auto nt = std::pair<T, StateTransition<T>>(event, StateTransition<T>(event, next_state));
            nt.second.addPredicate(predicate, arc_action);
            transitionmap.insert(nt);
            return;
        }
        st->second.addPredicate(predicate, arc_action);
    }
    /// Add a predicate callback which will be called whenever the named event is
    /// received. The result of the predicate call must be true for the transition to proceed.
    /// Otherwise the event is ignored, and no state change will take place.
    void addEventPredicate(T eventname, ActionBase *action)
    {
        // find the transition for this event
        auto t = transitionmap.find(eventname);
        if (t != transitionmap.end())
        {
            t->second.addPredicate(action);
        }
    }

    /// Add a callback when the State is entered. Note that this is substantially
    /// different than using an arc-transition action. Here *any* event which causes
    /// the *state* to be entered triggers this call. Arc-transitions are called
    /// only when a specific previous state,event and predicate set are satisfied.
    void addEnterAction(ActionBase *p)
    {
        enterAction.reset(p);
    }
    
    /// Add a callback when the State is exited. Note that this is substantially
    /// different than using an arc-transition action. Here *any* event which causes
    /// the *state* to be exited triggers this call. Arc-transitions are called
    /// only when a specific current state,event and predicate set are satisfied.
    void addLeaveAction(ActionBase *p)
    {
        leaveAction.reset(p);
    }

    T getName()
    {
        return state_name;
    }

    /// Call the exit action for the new state.
    void call_enter_action()
    {
        if (enterAction)
        {
            enterAction->do_action();
        }
    }

    /// Call the exit action for the previous state.
    void call_exit_action()
    {
        if (leaveAction)
        {
            leaveAction->do_action();
        }
    }

    const std::map<T, StateTransition<T> > & getTransitions()
    {
        return transitionmap;
    }
protected:
    std::map<T, StateTransition<T> > transitionmap;
    std::shared_ptr<ActionBase> enterAction, leaveAction;
    T state_name;
};

///
///
template<typename T>
class FiniteStateMachine
{
///
/// FiniteStateMachine (FSM) Implementation
/// =======================================
/// Concepts are the following:
/// * a FSM contains a number of possible states
/// * each State contains a mapping of input events to possible StateTransitions
/// * each StateTransition contains a list of user-defined predicates to be tested
/// to decide whether or not the transition should proceed. The predicates are contained
/// within the Action template.
/// * When a StateTransition causes a change of State, the user-defined enter/exit
/// actions are called to process the State change.
///
/// States and Events are denoted as strings.
///
/// Concrete Example:
/// -----------------
///
/// Consider a FSM model of a PC power supply controlled by a simple pushbutton.
/// It has three States:
/// * Off
/// * On
/// * Standby
///
/// Just like on a real PC, pushing the button when the PC is off causes the power to come on.
/// But once on, a quick button press places the PC in a 'hibernating' mode, with the power
/// supply in 'Standby'. A long press while on, does something different -- it causes the
/// supply to turn off all voltage.
///
/// So already we have described some behavior and transitions:
/// * While in state:  Given the event: Change state to:
/// * "Off":      "short_press":          "On"
/// * "Off":      "long_press":          "On"
/// * "On":       "long_press":          "Off"
/// * "On":      "short_press":          "Standby"
/// * "Standby":  "long_press":          "Off"
/// * "Standby":  "short_press":          "On"
///
/// See unit tests for the implementation of the example above.

public:
    FiniteStateMachine() : current_state(),
        prior_state(),
        initial_state()
    {}
    /// Cause a new empty state to be created
    void addState(T statename)
    {
        states[statename] = State<T>(statename);
    }

    /// Define a transition between states. If the States do not exist, they are created
    void addTransition(T from_state,
                       T event_name,
                       T to_state,
                       ActionBase *predicate = 0,
                       ActionBase *arc_action = 0)
    {
        auto s1 = states.find(from_state);
        if (s1 == states.end())
        {
            // state doesn't exist, create one.
            states[from_state] = State<T>(from_state);
            s1 = states.find(from_state);
        }
        s1->second.addTransition(event_name, to_state, predicate, arc_action);
    }
    /// Register a callback for when the named state is exited
    void addLeaveAction(T state_name, ActionBase *p)
    {
        auto st = states.find(state_name);
        if (st != states.end())
        {
            st->second.addLeaveAction(p);
        }
        else
        {
            std::cerr << "No such state:";
            std::cerr << state_name << std::endl;
        }
    }




    /// Register a callback for when the named state is entered
    void addEnterAction(T state_name, ActionBase *p)
    {
        auto st = states.find(state_name);
        if (st != states.end())
        {
            st->second.addEnterAction(p);
        }
        else
        {
            std::cerr << "No such state:";
            std::cerr << state_name << std::endl;
        }

    }
    /// Specify the initial state
    void setInitialState(T init)
    {
        // TBF should do a fsck check on the fsm, states, transitions etc.
        initial_state = init;
        current_state = init;
    }

    // These methods are commonly used at runtime to process events:
    /// send an event into the state machine. The return value indicates
    /// whether or not the event caused a state change.
    bool handle_event(T event)
    {
        if (!states[current_state].is_event_known(event))
        {
            return false;
        }
        T nxtstate;
        bool result = states[current_state].handle_event(event,nxtstate);
        if (!result)
        {
            // event unrecognized, or predicate failed
            return false;
        }
        // make sure the nxtstate is not invalid -- this shouldn't be possible
        if (states.find(nxtstate) == states.end())
        {
            std::cerr << "Error event " << event <<
                      " while in state " << current_state
                      << "places fsm in unknown state -- event ignored"
                      << std::endl;
            return false;
        }
        if (result)
        {
            // Ok transistion looks ok, call the enter/exit actions if they exist
            if (current_state == nxtstate)
            {
                // no state change, nothing to do
                return result;
            }
            states[current_state].call_exit_action();
            prior_state = current_state;
            current_state = nxtstate;
            states[current_state].call_enter_action();
        }
        return result;
    }

    /// Returns the name of the current state
    T getState()
    {
        return current_state;
    }

    /// Run checks on a fully built state machine to verify there
    /// are no "dead ends" (states with an entry but no exit)
    /// or unreachable states (states which can never be entered).
    /// Sort of like an filesystem check, but for the state machine.
    bool run_consistency_check()
    {
        bool check_passed = true;
        std::vector<T> targetlist;
        // Check for eventless states.
        for (auto s = states.begin(); s != states.end(); ++s)
        {
            auto transitionmap = s->second.getTransitions();
            if (transitionmap.size() == 0)
            {
                std::cerr << "Note: State " << s->second.getName() << " has no events"
                          << " and therefore cannot be exited" << std::endl;
                check_passed = false;
            }
            // In each state, check the transitionmap for valid target states
            for (auto tm = transitionmap.begin(); tm != transitionmap.end(); ++tm)
            {
                T target_state = tm->second.getNextState();
                if (states.find(target_state) == states.end())
                {
                    std::cerr << "Note: State " << s->second.getName() << " event "
                              << tm->first << " has target state "
                              << target_state << " which does not exist" << std::endl;
                    check_passed = false;
                }
                targetlist.push_back(target_state);
            }
        }
        for (auto s = states.begin(); s != states.end(); ++s)
        {
            // verify every state is in the targetlist
            if (find(targetlist.begin(), targetlist.end(), s->second.getName()) == targetlist.end() &&
                    s->second.getName() != initial_state)
            {
                std::cerr << "Note: State " << s->second.getName() << " is unreachable by any event"
                          << std::endl;
                check_passed = false;
            }
        }

        return check_passed;

    }

protected:
    typedef std::map<T, State<T> > Statemap;
    T current_state, prior_state, initial_state;
    Statemap states;
};

};
#endif

