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
              int portnum = 0, std::string keymaster = "");
    ~Publisher();

    bool publish_data(std::string key, std::string data);

  private:

    bool _load_config_file(std::string config);

    struct PubImpl;
    boost::shared_ptr<PubImpl> _impl;
};

#endif // _PUBLISHER_H_
