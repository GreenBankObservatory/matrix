/*******************************************************************
 *  BankMgrPublisher.h - Declares a ZMQ publisher (PUB/SUB model) for
 *  VEGAS bank data.
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

#if !defined(_PUBLISHER_H_)
#define _PUBLISHER_H_

#include "auto_ptr.h"
#include "Mutex.h"

#include <string>
#include <set>

/********************************************************************
 * class Publisher
 *
 * This is a class that provides a 0MQ publishing facility. Publisher
 * both publishes data as a ZMQ PUB, but also provides a REQ/REP
 *
 * Usage involves some setup in the
 *
 * int main(...)
 * {
 *     // Instantiate a Zero MQ context, usually one per process.
 *     // It will be used by any 0MQ service in this process.
 *     ZMQContext::Instance();
 *     // Instantiate the 0MQ publisher, where 'major' is the device
 *     // name, 'minor' is the subdevice name, and 'port' is the
 *     // TCP port.  (this last may disappear.)
 *     Publisher::Instance(major, minor, port);
 *     ... // rest of manager
 *     Publisher::RemoveInstance(); // clean up
 *     ZMQContext::RemoveInstance();
 *     return 0;
 * }
 *
 * Elsewhere in the code:
 *
 *     ...
 *     // obtain pointer to singleton
 *     Publisher pub = Publisher::Instance();
 *     pub->publish_sampler(major, minor, data_buf, buflen, data_descriptor);
 *
 * Note the use of the 'major' and 'minor' names would seem redundant
 * because the publisher already has these values; but there are
 * executables which are composed of several managers--a coordinator
 * and some children--all sharing the same publisher.  Passing in the
 * 'major' and 'minor' device names allow the publisher to compose the
 * proper subscription key, which is:
 *
 *   <major>.<minor>:<type>[:sampler/parameter name]
 *
 * where 'type' is 'P' for parameters, 'S' for samplers, 'Data' for
 * data, and 'Log' for log output; the sampler/parameter name is
 * mandatory for types 'P' and 'S' but not used if the type is 'Data'.
 *
 *******************************************************************/

#include <map>
#include <string>
#include <boost/shared_ptr.hpp>

class Publisher

{
  public:

    enum
    {
        INPROC = 0x01,
        IPC    = 0x02,
        TCP    = 0x04
    };


    Publisher(std::string component, int transports = INPROC,
              int portnum = -1, std::string keymaster = "");
    ~Publisher();

    bool publish_data(std::string key, std::string data);

  private:

    bool _load_config_file(std::string config);

    struct PubImpl;
    boost::shared_ptr<PubImpl> _impl;
};

#endif // _PUBLISHER_H_
