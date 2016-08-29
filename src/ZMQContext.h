/*******************************************************************
 *  ZMQContext.h - Definition of the ZeroMQ context singleton class.
 *  Modules that need the ZMQ context may obtain it from this class,
 *  rather than it being passed around as a parameter.
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

#if !defined(_ZMQCONTEXT_H_)
#define _ZMQCONTEXT_H_

#include "zmq.hpp"
#include "matrix/Mutex.h"

#include <memory>

class ZMQContext

{
  public:
    ~ZMQContext();

    zmq::context_t &get_context();

    static std::shared_ptr<ZMQContext> Instance();
    static void RemoveInstance();

  private:

    zmq::context_t _context;

    ZMQContext();

    static std::shared_ptr<ZMQContext> _instance;
    static Mutex _instance_lock;
};

#endif // _ZMQCONTEXT_H_
