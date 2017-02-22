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
#include "ResourceLockTest.h"
#include "matrix/ResourceLock.h"
#include "matrix/Time.h"
#include <sys/time.h>
#include <math.h>
#include <deque>
#include <vector>

using namespace std;
using namespace Time;

// test for approximate equivalent time
void ResourceLockTest::test_release()
{
    // test a simple resource release
    {
        ResourceLock p([]() { cout << "releasing single resource" << endl; });

        cout << "Allocated single resource" << endl;
    }

    // test an LIFO 'stack' of cleanup objects
    // should see resources allocated in 1,2; then released 2,1
    {
        std::deque< ResourceLock> locks;
        cout << "allocate resource 1" << endl;
        ResourceLock lock1([]() { cout << "releasing resource 1 " << endl; });
        cout << "allocate resource 2"  << endl;
        ResourceLock lock2([]() { cout << "releasing resource 2 " << endl; });
        locks.push_front(std::move(lock1));
        locks.push_front(std::move(lock2));
    }

    // test an FIFO ordered list of cleanup objects
    // should see resources allocated in 1,2; then released 1,2
    {
        std::vector< ResourceLock> locks;
        cout << "allocate resource 1" << endl;
        ResourceLock lock1([]() { cout << "releasing resource 1 " << endl; });
        cout << "allocate resource 2"  << endl;
        ResourceLock lock2([]() { cout << "releasing resource 2 " << endl; });
        locks.push_back(std::move(lock1));
        locks.push_back(std::move(lock2));
    }

}
