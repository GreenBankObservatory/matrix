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
#include<functional>

///
/// Documentation for the FiniteStateMachine. This provides a simple yet extensible
/// way to express a State machine. Code examples are given in the unit tests.
/// States and Events may be represented in a number of various data-types.
/// The most common choices are strings (easy to write), or some form of
/// enumeration (e.g. int or long). Other types may be used, as long as they
/// implement the required operations. (e.g. compare, assign, ostream etc.)
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
    using PredicateOperator = std::function<bool(bool, bool)>;
    virtual bool do_action()
    {
        std::cout << "ActionBase::do_action() called" << std::endl; 
        return false;
    }
    virtual PredicateOperator bin_operator()
    {
        return 0;
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
    bool do_action() override
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

template <typename T>
class Predicate : public ActionBase
{
public:
    typedef bool (T::*PredicateMethod)(void);

    T  *_object;
    PredicateMethod _predicate;
    PredicateOperator _operation; 

    /// Expression/predicate evaluating whether a transition should
    /// proceed or not. The default for a list of predicates is to
    /// 'and' each one. The first Predicate in the list will get
    /// a 
    Predicate(T *obj, PredicateMethod cb, 
              PredicateOperator op=std::logical_and<bool>() ) :
        _object(obj),
        _predicate(cb),
        _operation(op)
    {
    }
    ///
    /// Invoke a call to the user provided callback
    ///
    bool do_action() override
    {
        if (_object && _predicate)
        {
            return (_object->*_predicate)();
        }
        else
        {
            return false;
        }
    }
    PredicateOperator bin_operator() override
    {
        return _operation;
    }
};

using PredicateList = std::vector<ActionBase *>;
using ActionList    = std::vector<ActionBase *>;



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
        predicates(),
        arc_actions()
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
    /// Verify predicates are satisfied in preparation for a state transition.
    /// Each Predicate also stores a logical binary operation, which specifies
    /// how the result of the predicate should be combined with the prior result.
    /// Note that the first Predicate has no prior result, so its operation is
    /// ignored.
    bool check_predicates()
    {
        bool result = true;
        for (auto p = predicates.begin(); p!=predicates.end(); ++p)
        {
            // the first predicate has no prior result, so the operation is ignored
            if (p == predicates.begin())
            {
                result = (*p)->do_action();
            }
            else if ((*p)->bin_operator())
            {
                result = (*p)->bin_operator()(result, (*p)->do_action());
            }
        }
        return result;
    }

    /// Add predicates and actions for this transition arc.
    void addPredicate(PredicateList &predicateList, 
                      ActionList    &arc_actionList)
    {
        for (auto p = predicateList.begin(); p!= predicateList.end(); ++p)
        {
            predicates.push_back(std::shared_ptr<ActionBase>(*p));
        }
        for (auto a = arc_actionList.begin(); a!=arc_actionList.end(); ++a)
        {
            arc_actions.push_back(std::shared_ptr<ActionBase>(*a));
        }
    }
    /// calls registered action routines on a per-transition arc basis. 
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
        // search for the first transition which matches event
        // and also satisfies all of its predicates
        // NOTE: order of specification matters. This will walk
        // the transitions in order of FSM addition.
        auto tr = transitionmap.begin();
        for(; tr!=transitionmap.end(); ++tr)
        {
            if (tr->first == event && tr->second.check_predicates())
            {
                break;
            }
        }
        if (tr!=transitionmap.end())
        {
            // predicates check, we are going to follow the transition.
            // call the transition arc action method if it exists
            tr->second.call_arc_actions();
            nxt_state = tr->second.getNextState();
            return true;
        }
        return false; // no state change occurred

    }

    /// Query the state to check if any transition recognizes the event.
    bool is_event_known(T event)
    {
        if (transitionmap.find(event) == transitionmap.end())
        {
            // dont have any legal transitions for this event
            // This may not be an error!
            // std::cerr << "FSM: Event:" << event << " unrecognized -- no actions taken"
            //           << std::endl;
            return false;
        }
        return true;
    }

    /// Add a state to state transition, optionally attaching an Action to be
    /// called if the event causes a state change.
    void addTransition(T event, T next_state, PredicateList &predicateList, 
                                              ActionList    &arc_actions)
    {
        auto nt = std::make_pair(event, StateTransition<T>(event, next_state));
        auto st = transitionmap.insert(nt);
        st->second.addPredicate(predicateList, arc_actions);
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
    // Mostly for debugging ...
    const std::multimap<T, StateTransition<T> > & getTransitions()
    {
        return transitionmap;
    }
protected:
    std::multimap<T, StateTransition<T> > transitionmap;
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
        initial_state(),
        states(),
        sequence_event_specified(false)
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
        std::vector<ActionBase *> predList;
        if (predicate)
            predList.push_back(predicate);
        std::vector<ActionBase *> arc_actions;
        if (arc_action)
            arc_actions.push_back(arc_action);
        addTransition(from_state, event_name, to_state, predList, arc_actions);
    }
    /// Same as above, but with a slightly different syntax which makes the
    /// application specification a bit easier to read.
    void addTransition(T from_state,
                       T event_name,
                       T to_state,
                       std::vector<ActionBase *> &predicates,
                       std::vector<ActionBase *> &actions)
    {
        auto s1 = states.find(from_state);
        if (s1 == states.end())
        {
            // state doesn't exist, create one.
            //auto ns = std::make_pair(from_state, State<T>(from_state));
            //s1 = states.insert(ns);
            states[from_state] = State<T>(from_state);
            s1 = states.find(from_state);
        }    
        s1->second.addTransition(event_name, to_state, 
                                 predicates, actions); 
    }
    /// Same as above, be making it easier to specify when only
    /// zero or one actions are specified.
    void addTransition(T from_state,
                       T event_name,
                       T to_state,
                       std::vector<ActionBase *> &predicates,
                       ActionBase *arc_action = 0)
    {
        std::vector<ActionBase *> arc_actions;
        if (arc_action)
            arc_actions.push_back(arc_action);
        addTransition(from_state, event_name, to_state, predicates, arc_actions);
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
    
    /// Perform an exploration of the transitions of the current state.
    /// Inject the event matching the transition which satisfies all of
    /// its predicates. Place a warning if more than one possibility
    /// is found, and only use the first one. (Experimental method)
    bool sequence()
    {
        // Apply the 'sequence' or 'tick' event (specified elsewhere)
        if (sequence_event_specified)
        {
            return handle_event(sequence_event);
        }
        return false;
    }
    void specify_sequence_event(T seq_event)
    {
        sequence_event = seq_event;
        sequence_event_specified = true;
    }
    void reset_sequence_event()
    {
        sequence_event_specified = false;
    }

    /// These methods are commonly used at runtime to process events:
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
    
    /// A debug routine which just enumerates the currently defined states, events, next state
    bool show_fsm()
    {
        bool check_passed = true;
        std::vector<T> targetlist;
        // Check for eventless states.
        for (auto s = states.begin(); s != states.end(); ++s)
        {
            auto transitionmap = s->second.getTransitions();
            std::cout << "\tState: " << s->first << " has the following events/next states:\n";
            // In each state, check the transitionmap for valid target states
            for (auto tm = transitionmap.begin(); tm != transitionmap.end(); ++tm)
            {
                std::cout << "\t\tEvent " << tm->first << " Next State: " 
                          << tm->second.getNextState() << std::endl;
                          
            }
        }
    }


protected:
    typedef std::map<T, State<T> > Statemap; 
    T current_state, prior_state, initial_state;
    Statemap states;
    T sequence_event;
    bool sequence_event_specified;
};

};
#endif

