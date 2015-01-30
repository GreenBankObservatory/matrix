
#include<iostream>
#include <cppunit/TextTestRunner.h>
#include "StateTransitionTest.h"

using namespace std;


int main(int argc, char **argv)
{
    CppUnit::TextTestRunner runner;
    runner.addTest(StateTransitionTest::suite());   
    runner.run();
    return 0;
}
