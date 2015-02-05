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
#include "FiniteStateMachine.h"

using namespace std;
using namespace FSM;

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
    FiniteStateMachine fsm;
    fsm.addTransition("Off", "mpress", "On");
    fsm.addTransition("On", "hold", "Off"); 
    fsm.addTransition("On", "mpress", "On");   
    fsm.setInitialState("Off");
    
    CPPUNIT_ASSERT(fsm.getState() == "Off");
    fsm.handle_event("mpress");
    CPPUNIT_ASSERT(fsm.getState() == "On");
    CPPUNIT_ASSERT(fsm.handle_event("mpress") == true);
    CPPUNIT_ASSERT(fsm.getState() == "On");
    CPPUNIT_ASSERT(fsm.handle_event("hold") == true);
    CPPUNIT_ASSERT(fsm.getState() == "Off");
    CPPUNIT_ASSERT(fsm.handle_event("boom") == false);
    CPPUNIT_ASSERT(fsm.getState() == "Off");    
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
};

void StateTransitionTest::test_medium_fsm()
{
    FiniteStateMachine fsm;
    MyEasyCheck my;
    fsm.addTransition("Off", "mpress", "On");
    fsm.addTransition("On", "hold", "Off"); 
    fsm.addTransition("On", "mpress", "On");   
    fsm.setInitialState("Off");
    // Now add some things to do when the state changes
    fsm.addLeaveAction("Off", new Action<MyEasyCheck>(&my, &MyEasyCheck::exitOff) );
    fsm.addEnterAction("Off", new Action<MyEasyCheck>(&my, &MyEasyCheck::enterOff) );
    fsm.addLeaveAction("On", new Action<MyEasyCheck>(&my, &MyEasyCheck::exitOn) );
    fsm.addEnterAction("On", new Action<MyEasyCheck>(&my, &MyEasyCheck::enterOn) );
    
    CPPUNIT_ASSERT(fsm.getState() == "Off");
    fsm.handle_event("mpress");
    CPPUNIT_ASSERT(fsm.getState() == "On");
    CPPUNIT_ASSERT(fsm.handle_event("mpress") == true);
    CPPUNIT_ASSERT(fsm.getState() == "On");
    CPPUNIT_ASSERT(fsm.handle_event("hold") == true);
    CPPUNIT_ASSERT(fsm.getState() == "Off");
    CPPUNIT_ASSERT(fsm.handle_event("boom") == false);
    CPPUNIT_ASSERT(fsm.getState() == "Off");        
}

class MyPredicate
{
public:
    MyPredicate() : locked(true) {}
    bool unlock() { locked=false; }
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
    FiniteStateMachine fsm;
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
}




