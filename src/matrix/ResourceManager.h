/*******************************************************************
 ** ResourceManager.h - Implements Resource management class which
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

#ifndef ResourceManager_h
#define ResourceManager_h

#include <functional>

/// A resource management utility which can be used in place of
/// the pthread_cleanup_push/pop mechanism.
///

class ResourceManager
{
public:
    /// Construct a resource management object. The parameter should
    /// be a lambda with a signature of void(). See example below.
    ResourceManager(std::function<void()> _capture) :
    _the_release(_capture), _do_cleanup(true)
    {}

    /// Destruct and release resource by invoking the lambda and closure.
    ~ResourceManager() { release(); }

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
    ResourceManager(const ResourceManager &) = delete;
    ResourceManager() = delete;
    ResourceManager & operator=(const ResourceManager &) = delete;

    /// If somehow the resource gets cleaned up before the destructor
    /// is called, we can cancel out the calling of the lambda.
    void cancel_cleanup() { _do_cleanup = false; }

private:

    std::function<void()>  _the_release;
    bool _do_cleanup;
};

/**
 * Example:
 *
 * {
 *     int fd = open(...);
 *     ResourceManager fdmgr([&fd]() { if (fd>=0) close(fd); fd=-1; } );
 *     ...
 * }
 *
 * As the fdmgr object goes out of scope, its destructor will call close on
 * the captured fd.
 */


#endif
