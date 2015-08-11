/*******************************************************************
 ** TCondition.h - A condition variable template class
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
 *
 *******************************************************************/

#ifndef _TCONDITION_H_
#define _TCONDITION_H_

#include <sys/time.h>
#include <pthread.h>
#include <errno.h>

#include "Mutex.h"
#include "Time.h"

/****************************************************************//**
 * \class TCondition
 *
 *  This class is useful for simple (for now) condition variable
 *  applications.  For more complex applications requiring finer grained
 *  control of the condition and the action to be taken, use
 *  bare `pthread_cond_t`.
 *
 *  Typical use::
 *
 *      TCondition<int> tc(0);
 *
 *      // in thread A, wait for tc to become 5
 *      tc.wait(5);  // or tc.wait(5, 50000), time out after 50000 microseconds
 *
 *      // in thread B, set the condition and signal/broadcast it:
 *      tc.signal(5);  // or tc.broadcast(5)
 *
 *******************************************************************/

/// A template which implements a condition variable. This
/// template can be used to signal/wait on a value of type T,
/// or simply on a void signal.
template <typename T> class TCondition : public Mutex

{
  public:
  
    TCondition(T const &val);
    virtual ~TCondition();

    /// Access the current value. Locks are used but the internal value may
    /// change prior to being tested outside the class.
    void get_value(T &v);
        
    /// Access the current value without the use of a lock.
    T &value();
    
    /// Set the value atomically. See above, normal usage will use one arg. 
    /// A read-modify-write may use a false 2nd arg to indicate the lock is
    /// already held, and should not be required. This would be the
    /// case in a read-modify-write situation, as the the example:
    ///
    ///          cond.lock();
    ///          cond.wait_with_lock(3);
    ///          cond.set_value(0, false);
    ///          cond.signal();
    ///          cond.unlock();
    ///
    /// This would cause the waiting thread to wait until the internal
    /// value was 3, then it would reset it safely to the value 0.
    /// Note: We cannot use the method signal(T const &), as it would
    /// attempt to relock the mutex. We also cannot use a regular
    /// unlocking wait method followed by set_value(T) or signal(T)
    /// because of the race between the unlock in wait and relock in
    /// signal(T) or set_value(T).
    
    void set_value(T v, bool use_lock=true);

    /// wait forever for the value to become equal to s.
    /// Upon returning the internal mutex is unlocked.
    void wait(T const &s);
    
    /// wait with a timeout for the value to be equal to s.
    /// Upon returning the internal mutex is unlocked.
    /// Return value is true if the timeout was not reached,
    /// or false if a timeout or error occured.
    bool wait(T const &s, int usecs);
    
    /// wait without a timeout for the value to be equal to s.
    /// Upon returning, the internal mutex is locked, and
    /// the method unlock() must be used to unlock it.
    /// Return value is true if the timeout was not reached,
    /// or false otherwise.
    void wait_with_lock(T const &s);
    
    /// wait with a timeout for the value to be equal to s.
    /// Upon returning, the internal mutex is locked, and
    /// the method unlock() must be used to unlock it.
    /// Return value is true if the timeout was not reached,
    /// or false otherwise.
    bool wait_with_lock(T const &s, int usecs);

    /// Waits without checking the internal value. Upon returning
    /// the internal mutex is locked, and must be unlocked by calling
    /// the unlock() method. Return value is true if no timeout occured,
    /// and false otherwise.
    /// This is convienent to use when an external condition is checked.
    bool wait_locked_with_timeout(int usecs);
   
    /// Singal without changing the internal value    
    void signal();
    
    /// Set the value to 's' atomically and signal any waiters.
    void signal(T const &s);
    
    /// Broadcast without changing the internal value
    void broadcast();
    
    /// Set the value to 's' and send a broadcast
    void broadcast(T const &s);

  protected:

    TCondition(TCondition &);

    T _value;
    pthread_cond_t _cond;

};

/*****************************************************************//**
 * Constructor, sets the initial condition value and initializes
 * the internal pthread_cond_t object.
 *
 * @param val: The initial value, must be provided.
 *
 *********************************************************************/

template <typename T> TCondition<T>::TCondition(T const &val)
                                      : Mutex(), _value(val)

{
    pthread_cond_init(&_cond, NULL);

}

/*****************************************************************//**
 * Destroys the internal data member pthread_cond_t.
 *
 *********************************************************************/

template <typename T> TCondition<T>::~TCondition()

{
    pthread_cond_destroy(&_cond);

}


/****************************************************************//**
 * Allows a caller to get the value of the underlying condition variable.
 *
 *  @param v: The value buffer passed in by the caller.
 *
 *******************************************************************/

template <typename T> void TCondition<T>::get_value(T &v)

{
    lock();
    v = _value;
    unlock();

}

/****************************************************************//**
 * Returns a reference to the value data member.
 *
 * @return A reference to _value.
 *
 *******************************************************************/

template <typename T> T &TCondition<T>::value()

{
    return _value;
}

/****************************************************************//**
 * Allows a caller to set the value of the underlying condition
 * variable, without doing a broadcast/signal.
 *
 *  @param v: The value
 *  @param use_lock: Specifies whether or not the modification should
 *  occur as a locked or unlocked transaction.
 *
 *******************************************************************/

template <typename T> void TCondition<T>::set_value(T v, bool use_lock)

{
    if (use_lock)
    {
        lock();
        _value = v;
        unlock();
    } else {
        _value = v;
    }
}

/****************************************************************//**
 * Signals that state parameter has been updated.  This releases exactly
 * one thread waiting for the condition.  If no threads are waiting,
 * nothing happens.  If many threads are waiting, the one that is
 * released is not specified.
 *
 *******************************************************************/

template <typename T> void TCondition<T>::signal()

{
    pthread_cond_signal(&_cond);
}

/****************************************************************//**
 * Signals that state parameter has been updated.  This releases exactly
 * one thread waiting for the condition.  If no threads are waiting,
 * nothing happens.  If many threads are waiting, the one that is
 * released is not specified.
 *
 *  @param s: Updates the condition value and signals the
 *
 *******************************************************************/

template <typename T> void TCondition<T>::signal(T const &s)

{
    lock();
    _value = s;
    pthread_cond_signal(&_cond);
    unlock();

}

/****************************************************************//**
 * Broadcasts that state parameter has been updated.  Releases all
 * threads waiting for this condition.  If no threads are waiting,
 * nothing happens.
 *
 *******************************************************************/

template <typename T> void TCondition<T>::broadcast()

{
    pthread_cond_broadcast(&_cond);
}

/****************************************************************//**
 * Broadcasts that state parameter has been updated.  Releases all
 * threads waiting for this condition.  If no threads are waiting,
 * nothing happens.
 *
 *******************************************************************/

template <typename T> void TCondition<T>::broadcast(T const &s)

{
    lock();
    _value = s;
    pthread_cond_broadcast(&_cond);
    unlock();

}

/****************************************************************//**
 * Waits for a corresponding signal or broadcast.
 *
 * @param s: The condition value we are waiting for.
 * @param usecs: The timeout in microseconds.
 *
 * @return true if wait succeeded for the particular value.  false if it
 *         timed out.
 *
 *******************************************************************/

template <typename T> bool TCondition<T>::wait(T const &s, int usecs)

{
    bool rval = wait_with_lock(s, usecs);
    unlock();
    return rval;

}

/****************************************************************//**
 * Waits for a corresponding signal or broadcast.
 *
 * @param s: The condition value we are waiting for.
 *
 * @return true if wait succeeded for the particular value.  false if it
 *         timed out.
 *
 *******************************************************************/

template <typename T> void TCondition<T>::wait(T const &s)

{
    wait_with_lock(s);
    unlock();

}

/****************************************************************//**
 * Waits for a corresponding signal or broadcast, but does not unlock
 * itself. Allows the waiter to perform an action prior to
 * unlocking. Doing this allows the TCondition to be used in a fashion
 * similar to a raw POSIX condition variable.
 *
 * @param s: The condition value we are waiting for.
 *
 *******************************************************************/

template <typename T> void TCondition<T>::wait_with_lock(T const &s)

{
    lock();

    while (_value != s)
    {
        pthread_cond_wait(&_cond, &mutex);
    }
    // Do not unlock!
}

/****************************************************************//**
 * Waits for a corresponding signal or broadcast, then returns without
 * unlocking.
 *
 * @param s: The condition value we are waiting for.
 * @param usecs: The timeout in microseconds.
 *
 * @return true if wait succeeded for the particular value.  false if it
 *         timed out.
 *
 *******************************************************************/

template <typename T> bool TCondition<T>::wait_with_lock(T const &s, int usecs)

{
    timespec to;
    int status;
    bool rval = true;
    
    Time::Time_t time_to_return;
    
    time_to_return = Time::getUTC() + (usecs * (Time::Time_t)1000);
    Time::time2timespec(time_to_return, to);

    lock();

    while (_value != s)
    {
        status = pthread_cond_timedwait(&_cond, &mutex, &to);

        if (status == ETIMEDOUT)
        {
            rval = false;
            break;
        }
    }

    // Do not unlock!
    return rval;

}

// Wait with a timeout, but ignore the internal value.
// This allows the implementation of an external while loop.
// Note: Must be called with the condition already locked.
// Returns with condition locked.
template <typename T> bool TCondition<T>::wait_locked_with_timeout(int usecs)
{
    timespec to;
    int status;
    bool rval = true;

    Time::Time_t time_to_return;
    
    time_to_return = Time::getUTC() + (usecs * (Time::Time_t)1000);
    Time::time2timespec(time_to_return, to);
    
    status = pthread_cond_timedwait(&_cond, &mutex, &to);

    if (status == ETIMEDOUT)
    {
        rval = false;
    }
    // Do not unlock!
    return rval;
}

#endif
