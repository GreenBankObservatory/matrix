//# Copyright (C) 2002 Associated Universities, Inc. Washington DC, USA.
//#
//# This program is free software; you can redistribute it and/or modify
//# it under the terms of the GNU General Public License as published by
//# the Free Software Foundation; either version 2 of the License, or
//# (at your option) any later version.
//#
//# This program is distributed in the hope that it will be useful, but
//# WITHOUT ANY WARRANTY; without even the implied warranty of
//# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
//# General Public License for more details.
//#
//# You should have received a copy of the GNU General Public License
//# along with this program; if not, write to the Free Software
//# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//#
//# Correspondence concerning GBT software should be addressed as follows:
//#     GBT Operations
//#     National Radio Astronomy Observatory
//#     P. O. Box 2
//#     Green Bank, WV 24944-0002 USA

//# $Id: Thread.h,v 1.5 2008/02/29 15:05:48 rcreager Exp $

#ifndef THREAD_H
#define THREAD_H

#include <assert.h>
#include <pthread.h>

/**
 * \class Thread
 *
 * A simple wrapper for POSIX threads that allows a non-static class
 * member function to be given as the thread entry point.
 *
 * Example:
 *
 *     class foo
 *     {
 *     public:
 *         void bar();
 *
 *     private:
 *         Thread<foo> _bar_thread;
 *     }
 *
 *     ...
 *
 *     foo::foo() :
 *         _bar_thread(this, foo::bar)
 *     {
 *         _bar_thread.start();
 *     }
 *
 *     foo::~foo()
 *     {
 *         _bar_thread.stop_without_cancel();
 *     }
 *
 */

/// A base class to hold the thread creation hook.
/// The default is to do nothing. It is provided for other systems
/// (e.g. xenomai RTOS) to do some initialization at thread start.
/// To be safe, the set_thread_create_hook() should be called
/// prior to creating any threads.
class ThreadBase 
{
public:
    typedef void (*CreateHook) ();
    static void set_thread_create_hook(CreateHook h) 
    {
        thread_create_hook = h;
    }
protected:
    static CreateHook thread_create_hook;
};

template<typename T>
class Thread : public ThreadBase
{
    typedef Thread<T> THREAD;

    /// Redirect to member function.
    static void *thread_proc(void *thread)
    {
        if (thread_create_hook)
        {
            (thread_create_hook)();
        }
        return reinterpret_cast<THREAD *>(thread)->run();
    }

public:

    typedef void (T::*THREADPROC)();

    Thread(T *object_, THREADPROC proc_, size_t stacksize_ = 0);
    ~Thread();

    int start();
    bool running();
    void stop();
    void stop_without_cancel();

private:
    /// Redirect to the actual thread procedure.
    void *run()
    {
        (object->*proc)();
        return 0;
    }

    pthread_t  id;              ///< identifies the system thread
    T         *object;          ///< thread data
    THREADPROC proc;            ///< thread procedure
    size_t stacksize;           ///< user specified thread stack size
};


/**
 * Constructor takes T * and T::THREADPROC, ensuring that one object-one
 * thread. Also takes a defaulted stack size for the thread.  If the
 * default (0) is used, thread stack size defaults to that set by the system.
 *
 * @param object_: The class object for the class member function that
 * is the thread entry point.
 * @param proc_: The member function that is the thread entry point.
 * @param stacksize_ (optional) the thread stack size.
 *
 */

template<typename T> Thread<T>::Thread(T *object_, Thread<T>::THREADPROC proc_, size_t stacksize_)
    : id(0)
    , object(object_)
    , proc(proc_)
    , stacksize(stacksize_)
{
}

/**
 * Destructor. Stops the thread with a cancel. If you wish to stop the
 * thread in a more gentle way, first signal the thread to end, then
 * call `stop_without_cancel()` and wait for it to return. Doing so
 * allows the thread to clean up and terminate gracefully.
 *
 */

template<typename T> Thread<T>::~Thread()
{
    stop();
}

/**
 * Start the thread running.
 *
 * @return 0 on success, an error code on failure. (see man
 * `pthread_create()` for the error codes returned.)
 *
 */

template<typename T> int Thread<T>::start()
{
    int err;


    assert(0 != object);
    assert(0 != proc);
    assert(0 == id);

#if _POSIX_THREAD_ATTR_STACKSIZE
    if (stacksize && sysconf(_SC_THREAD_ATTR_STACKSIZE) > 0)
    {
        pthread_attr_t attr;

        if ((err = pthread_attr_init(&attr)) != 0)
        {
            return err;
        }

        if ((err = pthread_attr_setstacksize(&attr, stacksize)) != 0)
        {
            return err;
        }

        err = pthread_create(&id, &attr, thread_proc, this);
    }
    else
    {
        err = pthread_create(&id, 0, thread_proc, this);
    }
#else
    err = pthread_create(&id, 0, thread_proc, this);
#endif
    return err;
}

/**
 * Checks to see if the thread has been started and is running.
 *
 * @return true if the thread is running, false otherwise.
 *
 */

template<typename T> bool Thread<T>::running()

{
    return (0 != id);
}

/**
 * Stops the thread by cancelling it and joining on it. This is a severe
 * way to stop the thread as a cancel ends the thread immediately and
 * thus does not allow it to clean up. Use with caution.
 *
 */

template<typename T> void Thread<T>::stop()
{
    if (running())
    {
        pthread_cancel(id);
        pthread_join(id, 0);
        id = 0;
    }
}

/**
 * Does not issue a cancel to the thread, waits for the thread to end on
 * its own. This is a cleaner way to end the thread. For this to work
 * the parent thread must have some mechanism to signal this thread to
 * end, such as a TCondition, or a 0MQ pipe, etc.
 *
 */

template<typename T> void Thread<T>::stop_without_cancel()
{
    if (running())
    {
        pthread_join(id, 0);
        id = 0;
    }
}

#endif
