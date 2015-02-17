#include<iostream>
#include <cppunit/TextTestRunner.h>
#include "StateTransitionTest.h"
#include "TimeTest.h"
#include "ControllerTest.h"
#include "DataInterfaceTest.h"
#include "publisher_test.h"
#include "utility_test.h"
#include "keymaster_test.h"

using namespace std;


int main(int argc, char **argv)
{
    CppUnit::TextTestRunner runner;
    runner.addTest(StateTransitionTest::suite());
    runner.addTest(TimeTest::suite());
    // runner.addTest(DataInterfaceTest::suite());
    runner.addTest(ControllerTest::suite());
    runner.addTest(PublisherTest::suite());
    runner.addTest(UtilityTest::suite());
    runner.addTest(KeymasterTest::suite());
    runner.run();
    return 0;
}
