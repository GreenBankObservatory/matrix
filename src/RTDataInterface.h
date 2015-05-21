/*******************************************************************
 *  RTDataInterface.h - A DataInterface transport for real time, using
 *  semaphore queues.
 *
 *  Copyright (C) 2015 Associated Universities, Inc. Washington DC, USA.
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

#if !defined(_RTDATAINTERFACE_H_)
#define _RTDATAINTERFACE_H_

#include "DataInterface.h"
#include <string>

namespace matrix
{
    class RTTransportServer : public TransportServer
    {
    public:

        RTTransportServer(std::string keymaster_url, std::string key);
        virtual ~RTTransportServer();

    private:

        // The RTTransportClient needs access to these two functions.
        friend class RTTransportClient;

        bool _subscribe(std::string key, DataCallbackBase *cb);
        bool _unsubscribe(std::string key, DataCallbackBase *cb);

        bool _publish(std::string key, const void *data, size_t size_of_data);
        bool _publish(std::string key, std::string data);

        struct Impl;
        std::shared_ptr<Impl> _impl;
        friend class TransportServer;
        static TransportServer *factory(std::string, std::string);
        static std::map<std::string, RTTransportServer *> _rttransports;
    };

    class RTTransportClient : public TransportClient
    {
    public:

        RTTransportClient(std::string urn);
        virtual ~RTTransportClient();

    private:

        bool _connect(std::string urn = "");
        bool _disconnect();
        bool _subscribe(std::string key, DataCallbackBase *cb);
        bool _unsubscribe(std::string key, DataCallbackBase *cb);

        std::string _key;
        DataCallbackBase *_cb;

        friend class TransportClient;
        static TransportClient *factory(std::string);
    };

}

#endif
