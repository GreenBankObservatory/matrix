#include "FiniteStateMachine.h"

using namespace FSM;
using namespace std;


void FiniteStateMachine::addState(string statename)
{
    states[statename] = *(new State(statename));
}

void FiniteStateMachine::addTransition(std::string from_state,
                                       std::string event_name,
                                       std::string to_state,
                                       ActionBase *p)
{
    auto s1 = states.find(from_state);
    if (s1 == states.end())
    {
        // state doesn't exist, create one.
        auto ns=make_pair<std::string, State>(from_state, State(from_state));
        states.insert(ns);
        s1 = states.find(from_state);       
    }
    s1->second.addTransition(event_name, to_state, p);
}

void FiniteStateMachine::addEnterAction(std::string state_name,
                                        ActionBase *p)
{
    auto st = states.find(state_name);
    if (st != states.end())
    {
        st->second.addEnterAction(p);
    }
    else
    {
        printf("No such state:%s\n", state_name.c_str());
    }
}

void FiniteStateMachine::addLeaveAction(std::string state_name,
                                        ActionBase *p)
{
    auto st = states.find(state_name);
    if (st != states.end())
    {
        st->second.addLeaveAction(p);
    }
    else
    {
        printf("No such state:%s\n", state_name.c_str());
    } 
}


bool FiniteStateMachine::handle_event(std::string ev)
{
    if (!states[current_state].is_event_known(ev))
    {
        return false;
    }
    string nxtstate;
    bool result = states[current_state].handle_event(ev,nxtstate);
    if (!result)
    {
        // event unrecognized, or predicate failed
        return false;
    }
    // make sure the nxtstate is valid -- this shouldn't be possible
    if (states.find(nxtstate) == states.end())
    {
        printf("Error event %s while in state %s places fsm in unknown state -- ignored\n",
               ev.c_str(), current_state.c_str());
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

void FiniteStateMachine::setInitialState(std::string init)
{
    // TBF should do a fsck check on the fsm, states, transitions etc.
    initial_state = init;
    current_state = init;
}


State::State(std::string statename) :
    transitionmap(), enterAction(), leaveAction(),
    state_name(statename) 
{
}

State::State() :
    transitionmap(), enterAction(), leaveAction(),
    state_name("undefined") 
{
}

bool State::is_event_known(std::string s)
{
    return (transitionmap.find(s) != transitionmap.end());
}

bool State::handle_event(std::string event, string &nxt_state)
{
    auto tr = transitionmap.find(event);
    if (tr == transitionmap.end())
    {
        // dont have any legal transitions for this event
        return false;
    }
    if (tr->second.check_predicates())
    {
        nxt_state = tr->second.getNextState();
        return true;
    }
    return false; // TBF
}

void State::addEventPredicate(std::string eventname, ActionBase *action)
{
    // find the transition for this event
    auto t = transitionmap.find(eventname);
    if (t != transitionmap.end())
    {
        t->second.addPredicate(action);
    }
}

void State::addTransition(std::string event, std::string nxt_state, ActionBase *p)
{
    auto st = transitionmap.find(event);
    if (st == transitionmap.end())
    {
        auto nt = make_pair<string, StateTransition>(event, StateTransition(event, nxt_state));
        nt.second.addPredicate(p);
        transitionmap.insert(nt);
    }
}

void State::addEnterAction(ActionBase *p)
{
    enterAction.reset(p);
}

void State::addLeaveAction(ActionBase *p)
{
    leaveAction.reset(p);
}

void State::call_enter_action()
{
    if (enterAction)
    {
        enterAction->do_action();
    }
}

void State::call_exit_action()
{
    if (leaveAction)
    {
        leaveAction->do_action();
    }
}


void StateTransition::addPredicate(ActionBase *p)
{
    if (p)
    {
        predicates.push_back(shared_ptr<ActionBase>(p));
    }
}

bool StateTransition::check_predicates()
{
    bool result = true;
    for (auto p = predicates.begin(); result && p!=predicates.end(); ++p)
    {
        result = (*p)->do_action() && result;
    }
    return result;
}







