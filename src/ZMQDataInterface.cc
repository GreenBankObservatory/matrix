//# Copyright (C) 2015 Associated Universities, Inc. Washington DC, USA.
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


#include "ZMQDataInterface.h"
#include "tsemfifo.h"
#include "Thread.h"
#include "ZMQContext.h"
#include "zmq_util.h"
#include "matrix_util.h"
#include "netUtils.h"
#include "Keymaster.h"

#include <iostream>
#include <algorithm>
#include <functional>

#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>

#include <string.h>
#include <errno.h>


using namespace std;
using namespace std::placeholders;
using namespace mxutils;

/**
 * Creates a ZMQTransportServer, returning a TransportServer pointer to it. This
 * is a static function that can be used by TransportServer::create() to
 * create this type of object based solely on the transports provided to
 * create(). The caller of TransportServer::create() thus needs no specific
 * knowledge of ZMQTransportServer.
 *
 * @param km_urn: the URN to the keymaster.
 *
 * @param key: The key to query the keymaster. This key should point to
 * a YAML node that contains information about the data source. One of
 * the sub-keys of this node must be a key 'Specified', which returns a
 * vector of transports required for this data source.
 *
 * @return A TransportServer * pointing to the created ZMQTransportServer.
 *
 */

TransportServer *ZMQTransportServer::factory(string km_url, string key)
{
    TransportServer *ds = new ZMQTransportServer(km_url, key);
    return ds;
}

// Matrix will support ZMQTransportServer out of the box, so we can go ahead
// and initialize the static TransportServer::factories here, pre-loading it
// with the ZMQTransportServer supported transports. Library users who
// subclass their own TransportServer will need to call
// TransportServer::add_factory() somewhere in their code, prior to creating
// their custom TransportServer objects, to add support for custom
// transports.
map<string, TransportServer::factory_sig> TransportServer::factories =

{
    {"tcp",    &ZMQTransportServer::factory},
    {"ipc",    &ZMQTransportServer::factory},
    {"inproc", &ZMQTransportServer::factory}
};

Mutex TransportServer::factories_mutex;

/**
 * \class PubImpl is the private implementation of the ZMQTransportServer class.
 *
 */

struct ZMQTransportServer::PubImpl
{
    PubImpl(vector<string> urls);
    ~PubImpl();

    struct data_package
    {
        string key;
        string data;
    };

    enum
    {
        PUBLISH = 0,
        STATE,
        NUM_INTERFACES
    };

    void server_task();
    bool publish(string key, string data);
    vector<string> get_urls();

    Thread<PubImpl> _server_thread;
    TCondition<bool> _server_thread_ready;
    tsemfifo<data_package> _data_queue;
    string _hostname;

    vector<string> _publish_service_urls;

    zmq::context_t &_ctx;
};

/**
 * Constructor of the implementation class of ZMQTransportServer.  The
 * implementation class handles all the details; the ZMQTransportServer class
 * merely provides the external interface.
 *
 * @param urns: The desired URNs, as a vector of strings. If
 * only the transport is given, ephemeral URLs will be generated.
 *
 */

ZMQTransportServer::PubImpl::PubImpl(vector<string> urns)
:
    _server_thread(this, &ZMQTransportServer::PubImpl::server_task),
    _server_thread_ready(false),
    _data_queue(100),
    _ctx(ZMQContext::Instance()->get_context())

{
    int i = 0;

    // process the urns.
    _publish_service_urls.clear();
    _publish_service_urls.resize(urns.size());
    transform(urns.begin(), urns.end(), _publish_service_urls.begin(), &process_zmq_urn);
    auto str_not_empty = std::bind(not_equal_to<string>(), _1, string());

    if (!all_of(_publish_service_urls.begin(), _publish_service_urls.end(), str_not_empty))
    {
        throw CreationError("Cannot use one or more of the following transports", urns);
    }

    if (!_server_thread.running())
    {
        string error;

        if (_server_thread.start() != 0)
        {
            error = string("ZMQTransportServer data task was not started.");
        }
        else if (!_server_thread_ready.wait(true, 1000000))
        {
            error = string("ZMQTransportServer data task never reported ready.");
        }

        if (!error.empty())
        {
            throw CreationError(error);
        }
    }
}

/**
 * Signals the server thread that we're done, waiting for it to exit
 * on its own.
 *
 */

ZMQTransportServer::PubImpl::~PubImpl()

{
    _data_queue.release();
    _server_thread.stop_without_cancel();
}

/**
 * Returns the URLs bound to the services.
 *
 * @return A vector of two vectors of strings. The first element is the
 * state service URL set, the second is the publisher service URL set.
 *
 */

vector<string> ZMQTransportServer::PubImpl::get_urls()
{
    return _publish_service_urls;
}


/**
 * This is the publisher server task.  It sits on the queue waiting
 * for something to be published until it is told to stop.
 *
 */

void ZMQTransportServer::PubImpl::server_task()

{
    data_package dp;
    zmq::socket_t data_publisher(_ctx, ZMQ_PUB);
    string tcp_url;

    try
    {
        vector<string>::iterator urn;

        for (urn = _publish_service_urls.begin(); urn != _publish_service_urls.end(); ++urn)
        {
            boost::regex p_tcp("^tcp"), p_inproc("^inproc"), p_ipc("^ipc"), p_xs("X+$");
            boost::smatch result;

            // bind using tcp. If port is not given (port == 0), then use ephemeral port.
            if (boost::regex_search(*urn, result, p_tcp))
            {
                int port_used;

                if (!getCanonicalHostname(_hostname))
                {
                    cerr << "ZMQTransportServer: Unable to obtain canonical hostname: "
                         << strerror(errno) << endl;
                    return;
                }

                // ephem port requested? ("tcp://*:XXXXX")
                if (boost::regex_search(*urn, result, p_xs))
                {
                    port_used = zmq_ephemeral_bind(data_publisher, "tcp://*:*", 1000);
                }
                else
                {
                    data_publisher.bind(urn->c_str());
                    vector<string> parts;
                    boost::split(parts, *urn, boost::is_any_of(":"));
                    port_used = convert<int>(parts[2]);
                }

                // transmogrify the tcp urn to the actual urn needed for
                // a client to access the service:
                // 'tcp://<canonical_hostname>:<port>
                ostringstream tcp_url;
                tcp_url << "tcp://" << _hostname << ":" << port_used;
                *urn = tcp_url.str();
            }

            // bind using IPC or INPROC:
            if (boost::regex_search(*urn, result, p_ipc)
                || boost::regex_search(*urn, result, p_inproc))
            {
                // these are already in a form the client can use.
                data_publisher.bind(urn->c_str());
            }
        }
    }
    catch (zmq::error_t e)
    {
        cerr << "Error in publisher thread: " << e.what() << endl
             << "Exiting publishing thread." << endl;
        return;
    }

    _server_thread_ready.signal(true); // Allow constructor to move on

    while (_data_queue.get(dp))
    {
        /* Publish data. Use zero-copy:
           // string buf holds the data. A zmq_message_t is created that
           // merely points to the data buffer, and is given a size to
           // the buffer.
           zmq::message_t payload(buf.data(), buf.size(), NULL);
           data_publisher.send(payload) */

        try
        {
            z_send(data_publisher, dp.key, ZMQ_SNDMORE);
            zmq::message_t payload((void *)dp.data.data(), dp.data.size(), NULL);
            data_publisher.send(payload);
        }
        catch (zmq::error_t e)
        {
            cerr << "ZMQ exception in publisher thread: "
                 << e.what() << endl;
        }
    }

    // Done. Clean up.
    int zero = 0;
    data_publisher.setsockopt(ZMQ_LINGER, &zero, sizeof zero);
    data_publisher.close();
}

/**
 * This is the ZMQTransportServer's Data publishing facility.  It is a private
 * function that does all the work preparing data for publication.
 * The data and metadata are copied into a 'data_package' object and
 * posted on the publication semaphore queue. std::strings are used for
 * the data buffer, passed by value. Because std::strings use
 * copy-on-write (COW) this is not an expensive operation.
 *
 * NOTICE: If inproc, the client shouldn't count on the indefinite
 * existence of this buffer.
 *
 * @param key: the data key
 * @param data: the data buffer
 *
 * @return true if the data was succesfuly placed in the publication
 * queue, false otherwise. 'false' indicates that the semaphore queue is
 * full.
 *
 */

bool ZMQTransportServer::PubImpl::publish(string key, string data)
{
    data_package dp;

    dp.key = key;
    dp.data = data;
    return _data_queue.try_put(dp);
}

ZMQTransportServer::ZMQTransportServer(string keymaster_url, string key)
    : TransportServer(keymaster_url, key)
{
    try
    {
        Keymaster km(_km_url);
        vector<string> urns;
        urns = km.get_as<vector<string> >(_transport_key + ".Specified");

        // will throw CreationError if it fails.
        _impl.reset(new PubImpl(urns));

        // register the AsConfigured urns:
        urns = _impl->get_urls();
        km.put(_transport_key + ".AsConfigured", urns, true);
    }
    catch (KeymasterException e)
    {
        throw CreationError(e.what());
    }
}

ZMQTransportServer::~ZMQTransportServer()
{
    // releases queue, stops thread.
    _impl.reset();

    try
    {
        Keymaster km(_km_url);
        cout << km.get("root") << endl;

        km.del(_transport_key + ".AsConfigured");
    }
    catch (KeymasterException e)
    {
        // Just making sure no exception is thrown from destructor. The
        // Keymaster client could throw if the KeymasterServer goes away
        // prior to this call.
    }
}

bool ZMQTransportServer::_publish(string key, const void *data, size_t size_of_data)
{
    string data_buf(size_of_data, 0);
    memcpy((char *)data_buf.data(), data, size_of_data);
    return _impl->publish(key, data_buf);
}

bool ZMQTransportServer::_publish(string key, string data)
{
    return _impl->publish(key, data);
}


ZMQDataSink::ZMQDataSink() : DataSink()
{
}

ZMQDataSink::~ZMQDataSink()
{
}

bool ZMQDataSink::_connect(std::string urn_from_keymaster)
{
    cerr << "unimplemented method " << __func__ << " called" << endl;
    return false;
}

bool ZMQDataSink::_subscribe(std::string urn_from_keymaster)
{
    cerr << "unimplemented method " << __func__ << " called" << endl;
    return false;
}

bool ZMQDataSink::_unsubscribe(std::string urn_from_keymaster)
{
    cerr << "unimplemented method " << __func__ << " called" << endl;
    return false;
}

bool ZMQDataSink::_get(void *v, size_t &size_of_data)
{
    cerr << "unimplemented method " << __func__ << " called" << endl;
    return false;
}

bool ZMQDataSink::_get(std::string &data)
{
    cerr << "unimplemented method " << __func__ << " called" << endl;
    return false;
}
