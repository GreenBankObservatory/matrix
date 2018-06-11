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


#include "matrix/ZMQDataInterface.h"
#include "matrix/RTDataInterface.h"
#include "matrix/tsemfifo.h"
#include "matrix/Thread.h"
#include "matrix/ZMQContext.h"
#include "matrix/zmq_util.h"
#include "matrix/matrix_util.h"
#include "matrix/netUtils.h"
#include "matrix/Keymaster.h"

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

#define SUBSCRIBE   1
#define UNSUBSCRIBE 2
#define QUIT        3

namespace matrix
{

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
        {"tcp",      &ZMQTransportServer::factory},
        {"ipc",      &ZMQTransportServer::factory},
        {"inproc",   &ZMQTransportServer::factory},
        {"rtinproc", &RTTransportServer::factory}
    };

/**
 * Creates a ZMQTransportClient, returning a TransportClient pointer to it. This
 * is a static function that can be used by TransportClient::create() to
 * create this type of object based solely on the transports provided to
 * create(). The caller of TransportClient::create() thus needs no specific
 * knowledge of ZMQTransportClient.
 *
 * @param km_urn: the URN to the keymaster.
 *
 * @param key: The key to query the keymaster. This key should point to
 * a YAML node that contains information about the data source. One of
 * the sub-keys of this node must be a key 'Specified', which returns a
 * vector of transports required for this data source.
 *
 * @return A TransportClient * pointing to the created ZMQTransportClient.
 *
 */

    TransportClient *ZMQTransportClient::factory(string urn)
    {
        TransportClient *ds = new ZMQTransportClient(urn);
        return ds;
    }

// Matrix will support ZMQTransportClient out of the box, so we can go ahead
// and initialize the static TransportClient::factories here, pre-loading it
// with the ZMQTransportClient supported transports. Library users who
// subclass their own TransportClient will need to call
// TransportClient::add_factory() somewhere in their code, prior to creating
// their custom TransportClient objects, to add support for custom
// transports.
    map<string, TransportClient::factory_sig> TransportClient::factories =

    {
        {"tcp",      &ZMQTransportClient::factory},
        {"ipc",      &ZMQTransportClient::factory},
        {"inproc",   &ZMQTransportClient::factory},
        {"rtinproc", &RTTransportClient::factory}
    };

/**
 * \class PubImpl is the private implementation of the ZMQTransportServer class.
 *
 */

    struct ZMQTransportServer::PubImpl
    {
        PubImpl(vector<string> urls);
        ~PubImpl();

        bool publish(string key, string data);
        bool publish(string key, void const *data, size_t sze);
        vector<string> get_urls();

        string _hostname;
        vector<string> _publish_service_urls;

        zmq::context_t &_ctx;
        zmq::socket_t _pub_skt;
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
        _ctx(ZMQContext::Instance()->get_context()),
        _pub_skt(_ctx, ZMQ_PUB)

    {

        // process the urns.
        _publish_service_urls.clear();
        _publish_service_urls.resize(urns.size());
        transform(urns.begin(), urns.end(), _publish_service_urls.begin(), &process_zmq_urn);
        auto str_not_empty = std::bind(not_equal_to<string>(), _1, string());

        if (!all_of(_publish_service_urls.begin(), _publish_service_urls.end(), str_not_empty))
        {
            throw CreationError("Cannot use one or more of the following transports", urns);
        }

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

                        cerr << Time::isoDateTime(Time::getUTC())
                             << " -- ZMQTransportServer: Unable to obtain canonical hostname: "
                             << strerror(errno) << endl;
                        return;
                    }

                    // ephem port requested? ("tcp://*:XXXXX")
                    if (boost::regex_search(*urn, result, p_xs))
                    {
                        port_used = zmq_ephemeral_bind(_pub_skt, "tcp://*:*", 1000);
                    }
                    else
                    {
                        _pub_skt.bind(urn->c_str());
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
                    _pub_skt.bind(urn->c_str());
                }
            }
        }
        catch (zmq::error_t &e)
        {
            cerr << Time::isoDateTime(Time::getUTC())
                 << " -- Error in publisher thread: " << e.what() << endl
                 << "Exiting publishing thread." << endl;
            return;
        }

    }

/**
 * Signals the server thread that we're done, waiting for it to exit
 * on its own.
 *
 */

    ZMQTransportServer::PubImpl::~PubImpl()

    {
        int zero = 0;
        _pub_skt.setsockopt(ZMQ_LINGER, &zero, sizeof zero);
        _pub_skt.close();
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
 * Publishes the data, as represented by a string.
 *
 * @param key: The published key to the data.
 *
 * @param data: The data, contained in a std::string buffer
 *
 */

    bool ZMQTransportServer::PubImpl::publish(string key, string data)
    {
        return publish(key, data.data(), data.size());
    }

/**
 * Publishes the data, provided as a void * with a size parameter.
 *
 * @param key: The published key to the data.
 *
 * @param data: A void pointer to the buffer containing the data
 *
 * @param sze: The size of the data buffer
 *
 */

    bool ZMQTransportServer::PubImpl::publish(string key, void const *data, size_t sze)
    {
        bool rval = true;

        try
        {
            z_send(_pub_skt, key, ZMQ_SNDMORE, 0);
            z_send(_pub_skt, (const char *)data, sze, 0, 0);
        }
        catch (zmq::error_t &e)
        {
            cerr << Time::isoDateTime(Time::getUTC())
                 << " -- ZMQ exception in publisher: "
                 << e.what() << endl;
            rval = false;
        }

        return rval;
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
        catch (KeymasterException &e)
        {
            throw CreationError(e.what());
        }
    }

    ZMQTransportServer::~ZMQTransportServer()
    {
        // close pub socket.
        _impl.reset();

        try
        {
            Keymaster km(_km_url);
            km.del(_transport_key + ".AsConfigured");
        }
        catch (KeymasterException &e)
        {
            // Just making sure no exception is thrown from destructor. The
            // Keymaster client could throw if the KeymasterServer goes away
            // prior to this call.
        }
    }

    bool ZMQTransportServer::_publish(string key, const void *data, size_t size_of_data)
    {
        return _impl->publish(key, data, size_of_data);
    }

    bool ZMQTransportServer::_publish(string key, string data)
    {
        return _impl->publish(key, data);
    }

/**********************************************************************
 * Transport Client
 **********************************************************************/

    struct ZMQTransportClient::Impl
    {
        Impl() :
            _pipe_urn("inproc://" + gen_random_string(20)),
            _ctx(ZMQContext::Instance()->get_context()),
            _connected(false),
            _sub_thread(this, &ZMQTransportClient::Impl::sub_task),
            _task_ready(false)
        {}

        ~Impl()
        {
            disconnect();
        }

        bool connect(std::string url);
        bool disconnect();
        bool subscribe(std::string key, DataCallbackBase *cb);
        bool unsubscribe(std::string key);

        void sub_task();

        std::string _pipe_urn;
        std::string _data_urn;
        zmq::context_t &_ctx;
        bool _connected;
        Thread<ZMQTransportClient::Impl> _sub_thread;
        TCondition<bool> _task_ready;
        std::map<std::string, DataCallbackBase *> _subscribers;
    };

    bool ZMQTransportClient::Impl::connect(string urn)
    {
        _data_urn = urn;

        if (!_connected)
        {
            if (_sub_thread.start() == 0)
            {
                if (_task_ready.wait(true, 100000000) == false)
                {
                    cerr << Time::isoDateTime(Time::getUTC())
                         << " -- ZMQTransportClient for URN " << urn
                         << ": subscriber thread aborted." << endl;
                    return false;
                }

                _connected = true;
                return true;
            }
            else
            {
                cerr << Time::isoDateTime(Time::getUTC())
                     << " -- ZMQTransportClient for URN " << urn
                     << ": failure to start susbcriber thread."
                     << endl;
                return false;
            }
        }

        return false;
    }

    bool ZMQTransportClient::Impl::disconnect()
    {
        if (_connected)
        {
            zmq::socket_t pipe(_ctx, ZMQ_REQ);
            pipe.connect(_pipe_urn.c_str());
            z_send(pipe, QUIT, 0);
            int rval;
            z_recv(pipe, rval);
            _sub_thread.stop_without_cancel();
            _connected = false;
            return rval ? true : false;
        }

        return false;
    }

    bool ZMQTransportClient::Impl::subscribe(string key, DataCallbackBase *cb)
    {
        if (_connected)
        {
            zmq::socket_t pipe(_ctx, ZMQ_REQ);
            pipe.connect(_pipe_urn.c_str());
            z_send(pipe, SUBSCRIBE, ZMQ_SNDMORE);
            z_send(pipe, key, ZMQ_SNDMORE);
            z_send(pipe, cb, 0);
            int rval;
            z_recv(pipe, rval);
            return rval ? true : false;
        }

        return false;
    }

    bool ZMQTransportClient::Impl::unsubscribe(string key)
    {
        if (_connected)
        {
            zmq::socket_t pipe(_ctx, ZMQ_REQ);
            pipe.connect(_pipe_urn.c_str());
            z_send(pipe, UNSUBSCRIBE, ZMQ_SNDMORE);
            z_send(pipe, key, 0);
            int rval;
            z_recv(pipe, rval);
            return rval ? true : false;
        }

        return false;
    }

    void ZMQTransportClient::Impl::sub_task()
    {
        zmq::socket_t sub_sock(_ctx, ZMQ_SUB);
        zmq::socket_t pipe(_ctx, ZMQ_REP);
        vector<string>::const_iterator cvi;
        bool invalid_context = false;

        sub_sock.connect(_data_urn.c_str());
        pipe.bind(_pipe_urn.c_str());

        // we're going to poll. We will be waiting for subscription requests
        // (via 'pipe'), and for subscription data (via 'sub_sock').
        zmq::pollitem_t items [] =
            {
#if ZMQ_VERSION_MAJOR > 3
                { (void *)pipe, 0, ZMQ_POLLIN, 0 },
                { (void *)sub_sock, 0, ZMQ_POLLIN, 0 }
#else
                { pipe, 0, ZMQ_POLLIN, 0 },
                { sub_sock, 0, ZMQ_POLLIN, 0 }
#endif
            };

        _task_ready.signal(true);

        while (1)
        {
            try
            {
                zmq::poll(&items[0], 2, -1);

                if (items[0].revents & ZMQ_POLLIN) // the control pipe
                {
                    int msg;
                    z_recv(pipe, msg);

                    if (msg == SUBSCRIBE)
                    {
                        string key;
                        DataCallbackBase *f_ptr;
                        z_recv(pipe, key);
                        z_recv(pipe, f_ptr);

                        if (key.empty())
                        {
                            z_send(pipe, 0, 0);
                        }
                        else
                        {
                            _subscribers[key] = f_ptr;
                            sub_sock.setsockopt(ZMQ_SUBSCRIBE, key.c_str(), key.length());
                            z_send(pipe, 1, 0);
                        }
                    }
                    else if (msg == UNSUBSCRIBE)
                    {
                        string key;
                        z_recv(pipe, key);

                        if (key.empty())
                        {
                            z_send(pipe, 0, 0);
                        }
                        else
                        {
                            sub_sock.setsockopt(ZMQ_UNSUBSCRIBE, key.c_str(), key.length());

                            if (_subscribers.find(key) != _subscribers.end())
                            {
                                _subscribers.erase(key);
                            }

                            z_send(pipe, 1, 0);
                        }
                    }
                    else if (msg == QUIT)
                    {
                        z_send(pipe, 0, 0);
                        break;
                    }
                }

                // The subscribed data is handled here
                if (items[1].revents & ZMQ_POLLIN)
                {
                    string key;
                    zmq::message_t msg; // the data
                    int more;
                    size_t more_size = sizeof(more);
                    map<string, DataCallbackBase *>::const_iterator mci;
                    DataCallbackBase *f = NULL;

                    // get the key
                    z_recv(sub_sock, key);
                    mci = _subscribers.find(key);

                    // get callback registered to this key
                    if (mci != _subscribers.end())
                    {
                        f = mci->second;
                    }

                    // repeat for every possible frame containing
                    // data. The use case is for just one data frame;
                    // this will work for this, but if there are more
                    // than one data frames it will also work. Every
                    // subsequent data frame will call the same
                    // callback however.
                    sub_sock.getsockopt(ZMQ_RCVMORE, &more, &more_size);

                    while (more)
                    {
                        sub_sock.recv(&msg);

                        // execute only if we found a callback.
                        if (f)
                        {
                            f->exec(key, msg.data(), msg.size());
                        }

                        sub_sock.getsockopt(ZMQ_RCVMORE, &more, &more_size);
                    }
                }
            }
            catch (zmq::error_t &e)
            {
                string error = e.what();
                cerr << Time::isoDateTime(Time::getUTC())
                     << " -- ZMQTransportClient subscriber task: " << error << endl
                     << "URN for this task: " << _data_urn << endl;

                if (error.find("Context was terminated", 0) != string::npos)
                {
                    invalid_context = true;
                    break;
                }
            }
        }

        if (!invalid_context)
        {
            int zero = 0;
            pipe.setsockopt(ZMQ_LINGER, &zero, sizeof zero);
            pipe.close();
            zero = 0;
            sub_sock.setsockopt(ZMQ_LINGER, &zero, sizeof zero);
            sub_sock.close();
        }
    }


    ZMQTransportClient::ZMQTransportClient(string urn)
        : TransportClient(urn),
          _impl(new Impl())

    {
    }

    ZMQTransportClient::~ZMQTransportClient()
    {
        _impl->disconnect();
    }

    bool ZMQTransportClient::_connect()
    {
        return _impl->connect(_urn);
    }


    bool ZMQTransportClient::_disconnect()
    {
        return _impl->disconnect();
    }

    bool ZMQTransportClient::_subscribe(string key, DataCallbackBase *cb)
    {
        return _impl->subscribe(key, cb);
    }

    bool ZMQTransportClient::_unsubscribe(std::string key)
    {
        return _impl->unsubscribe(key);
    }

}
