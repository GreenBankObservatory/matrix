
#include<iostream>
#include <cppunit/TextTestRunner.h>
#include "StateTransitionTest.h"
#include "TimeTest.h"
#include "ControllerTest.h"

using namespace std;


int main(int argc, char **argv)
{
    CppUnit::TextTestRunner runner;
    runner.addTest(StateTransitionTest::suite());   
    runner.addTest(TimeTest::suite());   
    runner.addTest(ControllerTest::suite());   
    runner.run();
    return 0;
}
