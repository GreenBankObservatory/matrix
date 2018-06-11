/*******************************************************************
 *  RTDataInterface.cc - Implementation of a tsemfifo<T> transport.
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

#include "matrix/RTDataInterface.h"
#include "matrix/zmq_util.h"
#include "matrix/Keymaster.h"
#include "matrix/Mutex.h"
#include "matrix/ThreadLock.h"

#include <string>

using namespace std;
using namespace mxutils;

namespace matrix
{
/**
 * Creates a RTTransportServer, returning a TransportServer pointer to it. This
 * is a static function that can be used by TransportServer::create() to
 * create this type of object based solely on the transports provided to
 * create(). The caller of TransportServer::create() thus needs no specific
 * knowledge of RTTransportServer.
 *
 * @param km_urn: the URN to the keymaster.
 *
 * @param key: The key to query the keymaster. This key should point to
 * a YAML node that contains information about the data source. One of
 * the sub-keys of this node must be a key 'Specified', which returns a
 * vector of transports required for this data source.
 *
 * @return A TransportServer * pointing to the created RTTransportServer.
 *
 */

    TransportServer *RTTransportServer::factory(string km_url, string key)
    {
        TransportServer *ds = new RTTransportServer(km_url, key);
        return ds;
    }

    map<string, RTTransportServer *> RTTransportServer::_rttransports;

/**
 * \class Impl is the private implementation of the RTTransportServer class
 *
 */

    struct RTTransportServer::Impl
    {
        Impl(string urn);
        ~Impl();

        bool publish(string key, string data);
        bool publish(string key, void const *data, size_t sze);
        string get_urn();
        bool subscribe(string key, DataCallbackBase *cb);
        bool unsubscribe(string key, DataCallbackBase *cb);

        // This is how the server knows who its subscribers are. The
        // first template arg is a key, which is in the form
        // "component.data". So if component 'cat' emits data 'meow', the
        // key would be "cat.meow". This class needs some way to
        // populate this map. This is done via the 'subscribe' method.
        Mutex _client_mutex;
        multimap<string, DataCallbackBase *> _clients;
        string _urn;
    };

/**
 * Constructs an RTTransportServer implementation.
 *
 * @param urn: The specified URN for this transport. (will be "rtinproc")
 *
 */
    RTTransportServer::Impl::Impl(string urn)
    {
        _urn = urn + "://" + gen_random_string(20);
    }

/**
 * Destructor for an RTTransportServer implementation.
 *
 */

    RTTransportServer::Impl::~Impl()
    {
    }

/**
 * Published data by key, as a std::string.
 *
 * @param key: The data key
 *
 * @return returns `true` if pub succeeded, `false` otherwise.
 *
 */

    bool RTTransportServer::Impl::publish(string key, string data)
    {
        return publish(key, data.data(), data.size());
    }

/**
 * Publishes data by key, void * and size. Data from DataSinks are
 * published with a key that corresponds to the source component and
 * data, with the format 'source.data', where 'source' is the source
 * component's name, and 'data' is the named data stream. Each data
 * stream 'source.data' may have multiple clients. RTTransportServer,
 * like ZMQ based transports, may publish the same data to multiple
 * clients.
 *
 * @param key: The data key
 *
 * @param data: A pointer to the data
 *
 * @param sze: The size of the data buffer pointed to by 'data'
 *
 * @return true if publish succeeded, false otherwise. The call will
 * fail if there is no subscriber.
 *
 */

    bool RTTransportServer::Impl::publish(string key, void const *data, size_t sze)
    {
        bool rval = false;
        multimap<string, DataCallbackBase *>::iterator client;
        ThreadLock<Mutex> l(_client_mutex);

        l.lock();

        // Find all clients subscribed to the data represented by 'key'.
        for (client = _clients.equal_range(key).first;
             client != _clients.equal_range(key).second; ++client)
        {
            // call their callbacks with the data.
            client->second->exec(key, (void *)data, sze);
            rval = true;
        }

        return rval;
    }

/**
 * Returns the as-configured urn.
 *
 * @return A std::string containing the configured URN.
 *
 */

    string RTTransportServer::Impl::get_urn()
    {
        return _urn;
    }

/**
 * This private function provides a means for an RTTransportClient to
 * subscribe to this RTTransportServer. The RTTransportClient is a
 * friend class.
 *
 * It is the client class that provides the public subscription
 * interface, and in other transports it uses the transport itself to
 * subscribe (i.e. a 0MQ client calls setsockopt() on the client
 * socket to do this, but there is no such transport here. The
 * RTTransportServer is a virtual transport only, depositing data
 * directly into the client's tsemfifo queue).
 *
 * @param key: The data key
 *
 * @param cb: The client's callback functor.
 *
 * @return Always returns true.
 *
 */

    bool RTTransportServer::Impl::subscribe(string key, DataCallbackBase *cb)
    {
        ThreadLock<Mutex> l(_client_mutex);

        l.lock();
        _clients.insert(make_pair(key, cb));
        return true;
    }

/**
 * Unsubscribes to the data. This is a private function callable only
 * by this class and friend classes, including RTTransportClient. That
 * class provides the public subscribe interface, and forwards the
 * request to this class.
 *
 * @param key: The data key.
 *
 * @param cb: The callback functor pointer. This should match the one
 * in the 'clients' map, otherwise the function will not succeed.
 *
 * @return true if the unsubscribe succeeds, false otherwise. Failure
 * means that the client was not already subscribed.
 *
 */

    bool RTTransportServer::Impl::unsubscribe(string key, DataCallbackBase *)
    {
        bool rval = false;
        multimap<string, DataCallbackBase *>::iterator client;
        ThreadLock<Mutex> l(_client_mutex);

        l.lock();

        // Many clients may be registered for this data under
        // 'key'. Look for an exact match of key, cb:
        // Note: We delete all clients for a given key with the rtproc
        // transport because the unsubscribe call here only is
        // called once due to reference counting on the TC. -- JJB
        for (client = _clients.equal_range(key).first;
             client != _clients.equal_range(key).second; ++client)
        {
            _clients.erase(client);
            rval = true;
        }

        return rval;
    }

/**
 * Constructor for the RTTransportServer. Creates an instance of this
 * class and registers it with the Keymaster. It then saves a pointer
 * to itself in the static map '_rttransports' so that the
 * RTTransportClient can find it.
 *
 * @param keymaster_url: The keymaster URN.
 *
 * @param key: The data transport key that specifies the transport configuration.
 *
 */

    RTTransportServer::RTTransportServer(string keymaster_url, string key)
        : TransportServer(keymaster_url, key)
    {
        try
        {
            Keymaster km(_km_url);
            string urn;
            urn = km.get_as<vector<string> >(_transport_key + ".Specified").front();

            _impl.reset(new Impl(urn));
            urn = _impl->get_urn();
            vector<string> urns;
            urns.push_back(urn);
            km.put(_transport_key + ".AsConfigured", urns, true);
            // stash ourselves away in this map so that our clients
            // may find us by urn
            _rttransports[urn] = this;
        }
        catch (KeymasterException &e)
        {
            throw CreationError(e.what());
        }
    }

/**
 * Destroys the RTTransportServer object. This cleans up the Keymaster
 * entry, and removes itself from the '_rttransports' map.
 *
 */

    RTTransportServer::~RTTransportServer()
    {
        try
        {
            Keymaster km(_km_url);
            km.del(_transport_key + ".AsConfigured");

            map<string, RTTransportServer *>::iterator i;

            if ((i = _rttransports.find(_impl->get_urn())) != _rttransports.end())
            {
                _rttransports.erase(i);
            }

            _impl.reset();
        }
        catch (KeymasterException &e)
        {
        }
    }

    bool RTTransportServer::_subscribe(string key, DataCallbackBase *cb)
    {
        return _impl->subscribe(key, cb);
    }

    bool RTTransportServer::_unsubscribe(string key, DataCallbackBase *cb)
    {
        return _impl->unsubscribe(key, cb);
    }

/**
 * This private function provides the TransportServer 'publish()' functionality.
 *
 * @param key: The data key.
 *
 * @param data: A pointer to the data buffer to publish
 *
 * @param size_of_data: The size of the 'data' buffer in bytes.
 *
 * @return true on success, false otherwise. Failure simply indicates
 * that there is no client subscribed for this data.
 *
 */

    bool RTTransportServer::_publish(string key, const void *data, size_t size_of_data)
    {
        return _impl->publish(key, data, size_of_data);
    }

/**
 * This private function provides the TransportServer 'publish()' functionality.
 *
 * @param key: The data key.
 *
 * @param data: A std::string serving as the data buffer to publish.
 *
 * @return true on success, false otherwise. Failure simply indicates
 * that there is no client subscribed for this data.
 *
 */
    bool RTTransportServer::_publish(string key, string data)
    {
        return _impl->publish(key, data);
    }




    TransportClient *RTTransportClient::factory(string urn)
    {
        return new RTTransportClient(urn);
    }



/**
 * RTTranportClient constructor.
 *
 * @param urn: The fully formed URN of the TransportServer. Used to
 * subscribe to its services.
 *
 */

    RTTransportClient::RTTransportClient(string urn)
        : TransportClient(urn)
    {
    }


    RTTransportClient::~RTTransportClient()
    {
        _unsubscribe(_key, _cb);
    }

    bool RTTransportClient::_connect(string)
    {
        bool rval = false;

        if (!_key.empty() && _cb != NULL)
        {
            rval = _subscribe(_key, _cb);
        }

        return rval;
    }

    bool RTTransportClient::_disconnect()
    {
        bool rval = false;

        if (!_key.empty() && _cb != NULL)
        {
            rval = _unsubscribe(_key, _cb);
        }

        return rval;
    }

    bool RTTransportClient::_subscribe(string key, DataCallbackBase *cb)
    {
        bool rval = false;
        std::map<std::string, RTTransportServer *>::iterator server;

        _key = key;
        _cb = cb;

        if ((server = RTTransportServer::_rttransports.find(_urn))
            != RTTransportServer::_rttransports.end())
        {
            // got the RTTransportServer...
            rval = server->second->_subscribe(key, cb);
        }

        return rval;
    }

    bool RTTransportClient::_unsubscribe(string key, DataCallbackBase *cb)
    {
        bool rval = false;
        std::map<std::string, RTTransportServer *>::iterator server;

        _key = key;
        _cb = cb;

        if ((server = RTTransportServer::_rttransports.find(_urn))
            != RTTransportServer::_rttransports.end())
        {
            // got the RTTransportServer...
            rval = server->second->_unsubscribe(key, cb);
        }

        return rval;
    }
}



