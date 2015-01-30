
#ifndef StateTransitionTest_h
#define StateTransitionTest_h

#include <cppunit/extensions/HelperMacros.h>

class StateTransitionTest : public CppUnit::TestCase
{
    CPPUNIT_TEST_SUITE(StateTransitionTest);
    CPPUNIT_TEST(test_cbs);
    CPPUNIT_TEST(test_simple_fsm);
    CPPUNIT_TEST(test_medium_fsm);
    CPPUNIT_TEST(test_fancy_fsm);
    CPPUNIT_TEST_SUITE_END();
    
    public:
    void test_cbs();
    void test_simple_fsm();
    void test_medium_fsm();
    void test_fancy_fsm();
};


#endif
