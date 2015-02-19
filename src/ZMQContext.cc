/*******************************************************************
 *  ZMQContext.cc - implementation of singleton class ZMQContext. In
 *  the majority of cases only one ZeroMQ context is used per
 *  application.  This context is the thread that handles the ZMQ I/O.
 *  Setting this up as a singleton allows all threads in the
 *  application easy access to the context.
 *
 *  Copyright (C) 2012 Associated Universities, Inc. Washington DC, USA.
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

#include "ZMQContext.h"
#include "ThreadLock.h"

boost::shared_ptr<ZMQContext> ZMQContext::_instance;
Mutex ZMQContext::_instance_lock;

/****************************************************************//**
 * \class ZMQContext
 *
 * A class that encapsulates the 0MQ context into a singleton. In all
 * but special cases only one context is needed per application, and a
 * singleton is a good way to enforce this while allowing easy access
 * without a global variable.
 *
 * Examples:
 *
 *   * Initialize the 0MQ context. The `Instance()` method will
 *     construct a ZMQContext if it does not already exist, thus it may
 *     be called at a convenient time to force construction, even if
 *     the return value is not used:
 *
 *         ZMQContext::Instance();
 *
 *   * Use the context. Obtain the underlying `zmq::context` first, then
 *     use as usual:
 *
 *         zmq::context &ctx = ZMQContext::Instance()->get_context();
 *         zmq::socket_t pipe(ctx, ZMQ_PAIR);
 *         // etc.
 *
 *   * Destroy the context. The ZMQContext will eventually be cleaned up
 *     when it goes out of scope as the program terminates. However, if
 *     you need to control when it destroys the contained zmq::context
 *     object, you may call the static member function RemoveInstance():
 *
 *         ZMQContext::RemoveInstance(); // destroys the zmq::context.
 *
 *******************************************************************/

/****************************************************************//**
 * constructor for singleton class ZMQContext.  Though the constructor
 * is private it still does stuff, in this case initialize the ZMQ
 * context object that is its payload.
 *
 *******************************************************************/

ZMQContext::ZMQContext() : _context(1)

{
}

/****************************************************************//**
 * Destructor for ZMQ Context.  Doesn't need to do anything, as the
 * ZMQ context self-destructs at the end of the program.
 *
 *******************************************************************/

ZMQContext::~ZMQContext()

{
}

/****************************************************************//**
 * This static function will return a pointer to the single instance
 * of this class.  It will create the class if it doesn't yet exist.
 *
 * @return A pointer to ZMQContext
 *
 *******************************************************************/

ZMQContext *ZMQContext::Instance()

{
    ZMQContext *p;
    ThreadLock<Mutex> l(_instance_lock);

    l.lock();

    if (_instance.get() == 0)
    {
	_instance.reset(new ZMQContext());
    }

    p = _instance.get();
    l.unlock();

    return p;
}

/****************************************************************//**
 * This static function forcibly destroys this instance.
 *
 *******************************************************************/

void ZMQContext::RemoveInstance()

{
    ThreadLock<Mutex> l(_instance_lock);
    l.lock();
    _instance.reset();
    l.unlock();
}

/****************************************************************//**
 * Returns a reference to the actual ZMQ context contained within.
 *
 * @return zmq::context_t &, a reference to the ZMQ context.
 *
 *******************************************************************/

zmq::context_t &ZMQContext::get_context()

{
    return _context;
}
