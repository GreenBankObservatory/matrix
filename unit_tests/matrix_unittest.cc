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


#include <iostream>
#include <cppunit/TextTestRunner.h>
#include "StateTransitionTest.h"
#include "TimeTest.h"
#include "ArchitectTest.h"
#include "publisher_test.h"
#include "utility_test.h"
#include "keymaster_test.h"
#include "TransportTest.h"
#include "TSemfifoTest.h"
#include "matrix/Thread.h"
#include "matrix/ZMQContext.h"
#include "ResourceLockTest.h"

using namespace std;

shared_ptr<ZMQContext> ctx; // = ZMQContext::Instance();

int main(int argc, char **argv)
{
    ctx = ZMQContext::Instance();
    CppUnit::TextTestRunner runner;
    runner.addTest(ResourceLockTest::suite());
    runner.addTest(StateTransitionTest::suite());
    runner.addTest(TimeTest::suite());
//    runner.addTest(ArchitectTest::suite());
    runner.addTest(UtilityTest::suite());
//    runner.addTest(KeymasterTest::suite());
//    runner.addTest(TransportTest::suite());
    runner.addTest(TSemfifoTest::suite());
    runner.run();

    return 0;
}
