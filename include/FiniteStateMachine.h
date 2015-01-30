#ifndef FiniteStateMachine_h
#define FiniteStateMachine_h

#include<cstdio>
#include<vector>
#include<map>
#include<memory>
#include<string>

namespace FSM 
{
///
/// FiniteStateMachine Implementation
/// =================================
/// Concepts are the following:
/// * an FSM contains a number of possible states
/// * each State contains a mapping of input events to possible actions
/// * Each action contains a list of predicate 'hooks', which are
///   called/tested when an action is proposed.
///
/// Abstract Example:
/// -----------------
/// Consider a FSM which models a three state thingy. 
/// The thingy can have three states: Off, On, or Standby
/// The event 'turnOn' when in the Off or Standby states maps to
/// the action 'performTurnOn' which changes the current state to
/// 'On'.
/// Easy enough, but what if we need to evaluate some predicates prior
/// to allowing the transition? For this we can add action predicates.
/// When an action attempts to fire, all predicates in the list are
/// evaluated. If they all return true, then the action succeeds.
/// However, if any fail, then the action is rejected.
///

/// A base class to allow derived templated objects to be stored in
/// stl containers.
class ActionBase
{
public:
    virtual bool do_action() { return false; }
};


/// A wrapper template to register various actions (callbacks)
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

    // Call the user provided callback
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
                     
                    

// define an interface for all state objects
class State
{
public:
    State(std::string statename);
    State();
    bool handle_event(std::string event, std::string &nxt_state);
    bool is_event_known(std::string);
    void addTransition(std::string event, std::string next_state, ActionBase *pred=0);
    void addEventPredicate(std::string eventname, ActionBase *);
    void addEnterAction(ActionBase *);
    void addLeaveAction(ActionBase *);
    std::string getName() { return state_name; }
    void call_enter_action();
    void call_exit_action();
protected:
    std::map<std::string, StateTransition> transitionmap;
    std::shared_ptr<ActionBase> enterAction, leaveAction; 
    std::string state_name;
};


class FiniteStateMachine
{
public:
    FiniteStateMachine() : current_state("unknown"),
    prior_state("unknown"),
    initial_state("unknown")
    {}
    bool handle_event(std::string event);
    void addState(std::string statename);
    void addTransition(std::string from_state,
                       std::string event_name,
                       std::string to_state,
                       ActionBase *p = 0);
    void addLeaveAction(std::string leaving_state, ActionBase *p);
    void addEnterAction(std::string entering_state, ActionBase *p);
    
    std::string getState() { return current_state; }
    void setInitialState(std::string init);
        
protected:
    typedef std::map<std::string, State> Statemap;
    std::string current_state, prior_state, initial_state;
    Statemap states;
};

};
#endif

