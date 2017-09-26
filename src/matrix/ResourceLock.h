/*******************************************************************
 ** ResourceLock.h - Implements Resource management class which
 * can be used in place of the pthread_cleanup_push/pop mechanism.
 *
 *  Copyright (C) 2017 Associated Universities, Inc. Washington DC, USA.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Correspondence concerning GBT software should be addressed as follows:
 *  GBT Operations
 *  Green Bank Observatory
 *  P. O. Box 2
 *  Green Bank, WV 24944-0002 US
 *
 *******************************************************************/

#ifndef ResourceLock_h
#define ResourceLock_h

#include <functional>

/// A resource management utility which can be used in place of
/// the pthread_cleanup_push/pop mechanism.
///
namespace matrix
{
    class ResourceLock
    {
    public:
        /// Construct a resource management object. The parameter should
        /// be a lambda with a signature of void(). See example below.
        ResourceLock(std::function<void()> _capture) :
                _the_release(_capture), _do_cleanup(true)
        {
        }

        /// Destruct and release resource by invoking the lambda and closure.
        ~ResourceLock()
        {
            release();
        }

        /// Calls the resource releasing lambda
        void release()
        {
            if (_do_cleanup)
            {
                _the_release();
                _do_cleanup = false;
            }
        }

        // prevent ResourceManagement objects from being copied.
        ResourceLock(const ResourceLock &) = delete;

        ResourceLock() = delete;

        ResourceLock &operator=(const ResourceLock &) = delete;

        // Allow std::move() to work
        ResourceLock(ResourceLock &&p)
        {
            _the_release = p._the_release;
            p._the_release = nullptr;
            _do_cleanup = p._do_cleanup;
            p._do_cleanup = false;
        }

        /// If somehow the resource gets cleaned up before the destructor
        /// is called, we can cancel out the calling of the lambda.
        void cancel_cleanup()
        {
            _do_cleanup = false;
        }

    private:

        std::function<void()> _the_release;
        bool _do_cleanup;
    };

/**
 * Example:
 *
 * {
 *     int fd = open(...);
 *     ResourceLock fdmgr([&fd]() { if (fd>=0) close(fd); fd=-1; } );
 *     ...
 * }
 *
 * As the fdmgr object goes out of scope, its destructor will call close on
 * the captured fd.
 *
 * Example 2 - an order stack of resources
 *
 *      std::deque< ResourceLock> locks;
        cout << "allocate resource 1" << endl;
        ResourceLock lock1([]() { cout << "releasing resource1 " << endl; });

        cout << "allocate resource 2"  << endl;
        ResourceLock lock2([]() { cout << "releasing resource2 " << endl; });

        locks.push_front(std::move(lock1));
        locks.push_front(std::move(lock2));

        when locks goes out of scope, the ResourceLock members will be called
        from back to front (i.e. in a LIFO order). If FIFO ordering is
        intended, then use push_back().
 */

}; // namespace matrix
#endif
