/*******************************************************************
 ** ThreadLock.h
 *
 *  Copyright (C) 2004 Associated Universities, Inc. Washington DC, USA.
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
 *  National Radio Astronomy Observatory
 *  P. O. Box 2
 *  Green Bank, WV 24944-0002 USA
 *
 *******************************************************************/

#if !defined(_THREADLOCK_H_)
#define _THREADLOCK_H_

#include <assert.h>

/****************************************************************//**
 * \class ThreadLock
 *
 * A template accepting any object that has a lock() and an unlock()
 * member function.  ThreadLock assumes that the lock() and unlock()
 * functions return 0 for success (as would the underlying
 * pthread_mutex_(un)lock() functions).  The object will unlock() the
 * contained object when it goes out of scope, making for exception-safe
 * locking.
 *
 * Example:
 *
 *     Mutex mutex;
 *     ...
 *
 *     void foo()
 *     {
 *         ThreadLock<Mutex> tl(mutex);
 *
 *         if (tl.lock() == 0)
 *         {
 *             // do something
 *         }
 *     } // unlocks when foo goes out of scope
 *
 *******************************************************************/

template<typename X> class ThreadLock
{
  public:

    explicit ThreadLock (X &p);
    ~ThreadLock();


    int lock();
    int unlock();
    int last_error();

  private:

    X &_the_lock;
    bool locked;
    int rval;
};

/****************************************************************//**
 * Constructs a ThreadLock object that locks a type X.
 *
 * @param p: The object of type X to lock. Must have a lock() and
 * unlock() function.
 *
 *******************************************************************/

template<typename X> ThreadLock<X>::ThreadLock(X &p)
    :  _the_lock(p),
       locked(false),
       rval(0)
{}

/****************************************************************//**
 * Destructor. Unlocks the object X. This means X will always be
 * unlocked when this goes out of scope.
 *
 *******************************************************************/

template<typename X> ThreadLock<X>::~ThreadLock()
{
    unlock();
}

/****************************************************************//**
 * Locks the object X.
 *
 * @return 0 on success, error code otherwise (see documentation for
 * type X.)
 *
 *******************************************************************/

template<typename X> int ThreadLock<X>::lock()
{
    if ((rval = _the_lock.lock()) == 0)
    {
        locked = true;
    }

    return rval;
}

/****************************************************************//**
 * Unlocks the object X.
 *
 * @return 0 on success, error code otherwise. (see documentation for
 * type X.)
 *
 *******************************************************************/

template<typename X> int ThreadLock<X>::unlock()
{
    if (locked)
    {
        if ((rval = _the_lock.unlock()) == 0)
        {
            locked = false;
        }

        return rval;
    }

    return 0;
}

/****************************************************************//**
 * Returns the value last returned by `lock()` and `unlock()`
 *
 * @return This is an integer, the value returned by the last call to
 * `lock()` or `unlock()`.
 *
 *******************************************************************/

template<typename X> int ThreadLock<X>::last_error()
{
    return rval;
}

#endif
