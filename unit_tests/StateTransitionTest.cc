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
//       GBT Operations
//       National Radio Astronomy Observatory
//       P. O. Box 2
//       Green Bank, WV 24944-0002 USA


#include <cstdio>
#include "StateTransitionTest.h"
#include "matrix/FiniteStateMachine.h"

using namespace std;
using namespace matrix;
using namespace matrix::FSM;

class Someclass 
{
public:
    bool mycallback(void);
};

bool Someclass::mycallback(void)
{
    printf ("hello cb\n");
    return true;
}

void StateTransitionTest::test_cbs()
{
    Someclass p;
    Action<Someclass> xxx(&p, &Someclass::mycallback);
    CPPUNIT_ASSERT(xxx.do_action() == true);
}

/*********************************************/
// Simple state machine for a power supply.
// Momentary press while off turns it on, but requires
// a long press and hold to turn off.
void StateTransitionTest::test_simple_fsm()
{
    FiniteStateMachine<std::string> fsm;
    fsm.addTransition("Off", "mpress", "On");
    fsm.addTransition("On", "hold", "Off"); 
    fsm.addTransition("On", "mpress", "On");   
    fsm.setInitialState("Off");
    CPPUNIT_ASSERT(fsm.run_consistency_check());
    
    CPPUNIT_ASSERT(fsm.getState() == "Off");
    fsm.handle_event("mpress");
    CPPUNIT_ASSERT(fsm.getState() == "On");
    CPPUNIT_ASSERT(fsm.handle_event("mpress") == true);
    CPPUNIT_ASSERT(fsm.getState() == "On");
    CPPUNIT_ASSERT(fsm.handle_event("hold") == true);
    CPPUNIT_ASSERT(fsm.getState() == "Off");
    CPPUNIT_ASSERT(fsm.handle_event("boom") == false);
    CPPUNIT_ASSERT(fsm.getState() == "Off");    

    // Now try a int-implemented FSM:
    enum { Off, On, mpress, hold,boom };
    FiniteStateMachine<int> ifsm;
    ifsm.addTransition(Off, mpress, On);
    ifsm.addTransition(On, hold, Off); 
    ifsm.addTransition(On, mpress, On);   
    ifsm.setInitialState(Off);
    CPPUNIT_ASSERT(ifsm.run_consistency_check());
    
    CPPUNIT_ASSERT(ifsm.getState() == Off);
    ifsm.handle_event(mpress);
    CPPUNIT_ASSERT(ifsm.getState() == On);
    CPPUNIT_ASSERT(ifsm.handle_event(mpress) == true);
    CPPUNIT_ASSERT(ifsm.getState() == On);
    CPPUNIT_ASSERT(ifsm.handle_event(hold) == true);
    CPPUNIT_ASSERT(ifsm.getState() == Off);
    CPPUNIT_ASSERT(ifsm.handle_event(boom) == false);
    CPPUNIT_ASSERT(ifsm.getState() == Off);    
}
/*********************************************/
// Test an FSM with enter/leave actions for changing state
// 
class MyEasyCheck
{
public:
    bool exitOff()
    {
        printf("Leaving off state\n");
        return true;
    }
    bool enterOn()
    {
        printf("Entering on state\n");
        return true;
    }
    bool exitOn()
    {
        printf("Leaving on state\n");
        return true;
    }
    bool enterOff()
    {
        printf("Entering off state\n");
        return true;
    }
    
    bool handle_event(void) 
    { 
        printf("EasyChk\n");
        return true; 
    }
    bool off_to_on_arc()
    {
        printf("transitioning from off to on\n");
        return true;
    }
};

void StateTransitionTest::test_medium_fsm()
{
    // make a simple FSM for a PC power supply.
    // No predicates are used, but transition callbacks
    // and state entry/exit are demonstrated.
    
    FiniteStateMachine<std::string> fsm;
    MyEasyCheck my;
    fsm.addTransition("Off", "mpress", "On", 0,
                      new Action<MyEasyCheck>(&my, &MyEasyCheck::off_to_on_arc) );

    fsm.addTransition("On", "hold", "Off"); 
    fsm.addTransition("On", "mpress", "On");   
    fsm.addTransition("On", "short", "Off");
    fsm.setInitialState("Off");
    // Now add some things to do when the state changes
    fsm.addLeaveAction("Off", new Action<MyEasyCheck>(&my, &MyEasyCheck::exitOff) );
    fsm.addEnterAction("Off", new Action<MyEasyCheck>(&my, &MyEasyCheck::enterOff) );
    fsm.addLeaveAction("On",  new Action<MyEasyCheck>(&my, &MyEasyCheck::exitOn) );
    fsm.addEnterAction("On",  new Action<MyEasyCheck>(&my, &MyEasyCheck::enterOn) );
    CPPUNIT_ASSERT(fsm.run_consistency_check());
    
    CPPUNIT_ASSERT(fsm.getState() == "Off");
    
    // turn the power supply on by momentarily pressing the button
    CPPUNIT_ASSERT(fsm.handle_event("mpress") == true);
    CPPUNIT_ASSERT(fsm.getState() == "On");
    
    // once its on another momentary press keeps it on
    CPPUNIT_ASSERT(fsm.handle_event("mpress") == true);
    CPPUNIT_ASSERT(fsm.getState() == "On");
    
    // Now holding the button down for a longer time turns it off
    CPPUNIT_ASSERT(fsm.handle_event("hold") == true);
    CPPUNIT_ASSERT(fsm.getState() == "Off");
    
    // toss an unknown event at it
    CPPUNIT_ASSERT(fsm.handle_event("boom") == false);
    CPPUNIT_ASSERT(fsm.getState() == "Off");   
    fsm.handle_event("mpress");
    
    // verify alternate path from state On
    CPPUNIT_ASSERT(fsm.getState() == "On");
    
    // now overload the power supply, it turns off to protect itself
    CPPUNIT_ASSERT(fsm.handle_event("short") == true);
    CPPUNIT_ASSERT(fsm.getState() == "Off");
}

class MyPredicate
{
public:
    MyPredicate() : locked(true) {}
    void unlock() { locked=false; }
    bool checkOffOn()
    {
        printf("Ok to turn on\n");
        return true;
    }
    bool checkOnOff()
    {
        if (locked)
            printf("Not ok to turn off\n");
        else
            printf("Ok to turn off\n");
        return !locked;
    }
    bool locked;
};

void StateTransitionTest::test_fancy_fsm()
{
    FiniteStateMachine<std::string> fsm;
    MyPredicate mychk;
    // example use of heap allocated predicate/action
    shared_ptr<MyEasyCheck> my;    
    my.reset(new MyEasyCheck());
    
    
    fsm.addTransition("Off", "mpress", "On", 
                      new Action<MyPredicate>(&mychk, &MyPredicate::checkOffOn));
    fsm.addTransition("On", "hold", "Off",
                      new Action<MyPredicate>(&mychk, &MyPredicate::checkOnOff)); 
    fsm.addTransition("On", "mpress", "On");   
    fsm.setInitialState("Off");

    
    // Now add some things to do when the state changes
    fsm.addLeaveAction("Off", new Action<MyEasyCheck>(my.get(), &MyEasyCheck::exitOff) );
    fsm.addEnterAction("Off", new Action<MyEasyCheck>(my.get(), &MyEasyCheck::enterOff) );
    fsm.addLeaveAction("On", new Action<MyEasyCheck>(my.get(), &MyEasyCheck::exitOn) );
    fsm.addEnterAction("On", new Action<MyEasyCheck>(my.get(), &MyEasyCheck::enterOn) );
    CPPUNIT_ASSERT(fsm.run_consistency_check());
    
    CPPUNIT_ASSERT(fsm.getState() == "Off");
    fsm.handle_event("mpress");
    CPPUNIT_ASSERT(fsm.getState() == "On");
    CPPUNIT_ASSERT(fsm.handle_event("mpress") == true);
    CPPUNIT_ASSERT(fsm.getState() == "On");
    CPPUNIT_ASSERT(fsm.handle_event("hold") == false);
    CPPUNIT_ASSERT(fsm.getState() == "On");
    mychk.unlock();
    
    CPPUNIT_ASSERT(fsm.handle_event("hold") == true);
    CPPUNIT_ASSERT(fsm.getState() == "Off");       
    fsm.show_fsm();
}

void StateTransitionTest::test_consistency_check()
{
    FiniteStateMachine<std::string> fsm;
    fsm.addState("S1");
    fsm.addState("S0");
    fsm.setInitialState("S0");
    // should fail with 3 notices:
    // * No exit transitions for both states
    // * S1 is unreachable
    CPPUNIT_ASSERT(fsm.run_consistency_check() == false);
    // Add a transition into S1
    fsm.addTransition("S0", "E1", "S1");
    // should still fail since we cant leave S1
    CPPUNIT_ASSERT(fsm.run_consistency_check() == false);
    fsm.addTransition("S1", "E2", "S0");
    // now it should be ok
    CPPUNIT_ASSERT(fsm.run_consistency_check() == true);
    fsm.addTransition("S1", "E3", "S1");
    CPPUNIT_ASSERT(fsm.run_consistency_check() == true);
    fsm.show_fsm();
    
    enum Test { S0, S1, E1, E2, E3 };
    FiniteStateMachine<uint32_t> ifsm;
    ifsm.addState(S1);
    ifsm.addState(S0);
    ifsm.setInitialState(S0);
    // should fail with 3 notices:
    // * No exit transitions for both states
    // * S1 is unreachable
    CPPUNIT_ASSERT(ifsm.run_consistency_check() == false);
    // Add a transition into S1
    ifsm.addTransition(S0, E1, S1);
    // should still fail since we cant leave S1
    CPPUNIT_ASSERT(ifsm.run_consistency_check() == false);
    ifsm.addTransition(S1, E2, S0);
    // now it should be ok
    CPPUNIT_ASSERT(ifsm.run_consistency_check() == true);
    ifsm.addTransition(S1, E3, S1);
    CPPUNIT_ASSERT(ifsm.run_consistency_check() == true);
    
}

class Sequencer
{
public:
    bool do_activating() { cout << "activating" << endl; return true; }
    bool do_commit() { cout << "arming" << endl; return true; }
    bool do_start() { cout << "starting" << endl; return true; }
    bool do_complete() { cout << "to ready" << endl; return true;}
    bool do_abort() { cout << "aborting" << endl; return true;}
    bool do_stop() { cout << "stopping" << endl; return true;}
    bool do_powerfail() { cout << "power failed" << endl; return true;}
    bool do_system_ok() { cout << "system ok" << endl; return true;}
    
    bool system_ok() { return sys_ok && power_ok; }
    bool start_time_reached() { return t > tstart; }
    bool stop_time_reached()  { return t > tstop; }
    bool commit_time_reached() { return t> tcommit; }
    bool has_error() { return error; }
    bool has_no_error() { return !has_error(); }
    bool power_failure() { return !power_ok; }
    bool user_stopped() { return user_stop; }
    
    bool error;
    bool sys_ok;
    bool power_ok;
    bool user_stop;
    double tstart, tstop, tcommit, t;
};

void StateTransitionTest::test_sequence_fsm()
{
    {
    FiniteStateMachine<std::string> fsm;
    auto seq = new Sequencer;
    
    vector<ActionBase *> predicates;
    predicates.push_back( new Action<Sequencer>(seq, &Sequencer::system_ok) );
    predicates.push_back( new Action<Sequencer>(seq, &Sequencer::has_no_error) );
    predicates.clear(); 
    fsm.addTransition("READY", "ACTIVATE", "ACTIVATING",
                      predicates, 
                      new Action<Sequencer>(seq, &Sequencer::do_activating));    
    fsm.addTransition("ACTIVATING", "TICK", "ARMING",
                new Predicate<Sequencer>(seq, &Sequencer::commit_time_reached),
                new Action<Sequencer>(seq, &Sequencer::do_commit) );
                
    fsm.addTransition("ARMING", "TICK", "RUNNING",
                new Predicate<Sequencer>(seq, &Sequencer::start_time_reached),
                new Action<Sequencer>(seq, &Sequencer::do_start) );
                
    // stop if stoptime is reached or user_stopped scan
    predicates.push_back( new Predicate<Sequencer>(seq, &Sequencer::stop_time_reached) );
    predicates.push_back( new Predicate<Sequencer>(seq, &Sequencer::user_stopped,
                          std::logical_or<bool>() ) );    
    fsm.addTransition("RUNNING", "TICK", "STOPPING",
                predicates,
                new Action<Sequencer>(seq, &Sequencer::do_stop) );
    predicates.clear();
    
    fsm.addTransition("RUNNING", "TICK", "POWERFAIL",
                new Predicate<Sequencer>(seq, &Sequencer::power_failure),
                new Action<Sequencer>(seq, &Sequencer::do_powerfail) ); 
                               
    fsm.addTransition("STOPPING", "TICK", "READY", 0,
                new Action<Sequencer>(seq, &Sequencer::do_complete) );
                
    fsm.addTransition("RUNNING", "ABORT", "ABORTING",
                new Predicate<Sequencer>(seq, &Sequencer::has_error),
                new Action<Sequencer>(seq, &Sequencer::do_abort) );  
                              
    fsm.addTransition("RUNNING", "TICK", "RUNNING",
                new Predicate<Sequencer>(seq, &Sequencer::has_no_error),
                new Action<Sequencer>(seq, &Sequencer::do_system_ok) ); 
                
    predicates.push_back( new Predicate<Sequencer>(seq, &Sequencer::system_ok) );
    predicates.push_back( new Predicate<Sequencer>(seq, &Sequencer::has_no_error) );         
    fsm.addTransition("ABORTING", "TICK", "READY", 
                predicates,
                new Action<Sequencer>(seq, &Sequencer::do_complete) );
    predicates.clear();
    
    fsm.addTransition("POWERFAIL", "POWERON", "READY", 
                new Predicate<Sequencer>(seq, &Sequencer::system_ok),
                new Action<Sequencer>(seq, &Sequencer::do_complete) );
                
    //intentional dup: (doesn't add anything ... just testing for issues)
    fsm.addTransition("POWERFAIL", "POWERON", "READY", 
                new Predicate<Sequencer>(seq, &Sequencer::system_ok),
                new Action<Sequencer>(seq, &Sequencer::do_complete) );
                                     
    fsm.addTransition("POWERFAIL", "TICK", "READY", 
                new Predicate<Sequencer>(seq, &Sequencer::system_ok),
                new Action<Sequencer>(seq, &Sequencer::do_complete) ); 
                                
    CPPUNIT_ASSERT( fsm.run_consistency_check() );
    fsm.show_fsm();
    
    fsm.setInitialState("READY");
    seq->t = 1;
    seq->tcommit = 2;
    seq->tstart = 3;
    seq->tstop = 4;
    seq->power_ok = true;
    seq->sys_ok = true;
    seq->user_stop = false;
    fsm.specify_sequence_event("TICK");
    CPPUNIT_ASSERT( !fsm.sequence() );
    CPPUNIT_ASSERT( fsm.handle_event("ACTIVATE") );
    CPPUNIT_ASSERT( fsm.getState() == "ACTIVATING" );
    CPPUNIT_ASSERT( !fsm.sequence() );
    seq->t++;
    CPPUNIT_ASSERT( !fsm.sequence() );
    seq->t++;
    CPPUNIT_ASSERT( fsm.sequence() );
    CPPUNIT_ASSERT( fsm.getState() == "ARMING" );
    CPPUNIT_ASSERT( !fsm.sequence() );
    seq->t++;
    // RUNNING has a self-transition for TICK.
    CPPUNIT_ASSERT( fsm.sequence() );
    CPPUNIT_ASSERT( fsm.getState() == "RUNNING" );
    CPPUNIT_ASSERT( fsm.sequence() );
    seq->power_ok = false;
    CPPUNIT_ASSERT( fsm.sequence() );
    CPPUNIT_ASSERT( fsm.getState() == "POWERFAIL" );
    CPPUNIT_ASSERT( !fsm.sequence() );
    seq->power_ok = true;
    CPPUNIT_ASSERT( fsm.sequence() );
    CPPUNIT_ASSERT( fsm.getState() == "READY" );

    // get back into running to test logical_or
    seq->t = 1;
     
    CPPUNIT_ASSERT( fsm.handle_event("ACTIVATE") );
    CPPUNIT_ASSERT( fsm.getState() == "ACTIVATING" );
    CPPUNIT_ASSERT( !fsm.sequence() );
    seq->t++;
    CPPUNIT_ASSERT( !fsm.sequence() );
    seq->t++;
    CPPUNIT_ASSERT( fsm.sequence() );
    CPPUNIT_ASSERT( fsm.getState() == "ARMING" );
    CPPUNIT_ASSERT( !fsm.sequence() );
    seq->t++;
    // RUNNING has a self-transition for TICK.
    CPPUNIT_ASSERT( fsm.sequence() );
    CPPUNIT_ASSERT( fsm.getState() == "RUNNING" );
    CPPUNIT_ASSERT( fsm.sequence() );
    seq->user_stop = true;
    CPPUNIT_ASSERT( fsm.sequence() );
    cout << fsm.getState() << endl;
    CPPUNIT_ASSERT( fsm.getState() == "STOPPING" );
    
    }
    cout << "test_fsm_complete" << endl;
    
}
    


