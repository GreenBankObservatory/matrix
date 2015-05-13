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


#include "DataInterface.h"
#include "Keymaster.h"
#include "ThreadLock.h"
#include "matrix_util.h"

#include <iostream>

using namespace std;
using namespace mxutils;

// This is the transport repository. A map of map of transport
// shared_ptr. The first map is keyed by component, and the value is
// another map, keyed by transport name, whose value is a shared_ptr for
// that transport.

namespace matrix
{

    TransportServer::component_map_t TransportServer::transports;
    TransportClient::client_map_t TransportClient::transports;
    Mutex TransportServer::factories_mutex;
    Mutex TransportClient::factories_mutex;

/**
 * This static function adds a factory function pointer to the
 * static 'factories' map of DataSource. This enables
 * `DataSource::create()` to create the correct DataSource type to
 * handle the transports. This function should be called by every
 * derived class implementation.
 *
 * @param transports: All the transports handled by the type returned by
 * 'factory'.
 *
 * @param factory: The factory that will return the correct type for the
 * transport(s). If multiple transports are given the factory function
 * pointer will be recorded for each of these transports. 'create()' can
 * then use this information to ensure that all transports given are
 * compatible.
 *
 */

    void TransportServer::add_factory(vector<string> transports, TransportServer::factory_sig factory)
    {
        ThreadLock<decltype(factories_mutex)> l(factories_mutex);
        vector<string>::const_iterator i;

        l.lock();

        for (i = transports.begin(); i != transports.end(); ++i)
        {
            factories[*i] = factory;
        }
    }

/**
 * Returns a shared_ptr to a TransportServer, creating a TransportServer
 * first if one does not exist for the keys given.
 *
 * @param km_urn: The URN to the Keymaster.
 *
 * @param component_name: The name, as givne in the Keymaster's YAML
 * store, of the component.
 *
 * @param transport_name: The name of the transport.
 *
 * @return A shared_ptr to the transport object that was requested.
 *
 */

    shared_ptr<TransportServer> TransportServer::get_transport(string km_urn,
                                                               string component_name,
                                                               string transport_name)
    {
        ThreadLock<decltype(transports)> l(transports);
        component_map_t::iterator cmi;
        transport_map_t::iterator tmi;

        l.lock();

        if ((cmi = transports.find(component_name)) == transports.end())
        {
            transports[component_name] = transport_map_t();
        }

        cmi = transports.find(component_name);

        if ((tmi = cmi->second.find(transport_name)) != cmi->second.end())
        {
            // found!
            return tmi->second;
        }
        else
        {
            // not found, must build and add, then return.
            string transport_key = "components." + component_name + ".Transports." + transport_name;
            cmi->second[transport_name] = TransportServer::create(km_urn, transport_key);
            return cmi->second[transport_name];
        }
    }

/**
 * Creates the correct type of TransportServer.
 *
 * This function is given the urn for the keymaster, and a transport
 * key. Using this key it obtains the transport information, which is
 * then used to create the correct TransportServer object for that transport.
 *
 * @param km_urn: the URN for the keymaster
 *
 * @return A shared_ptr<TransportServer> for the created object.
 *
 */

    template <typename T>
    struct same_as
    {
        same_as(T v) : _v(v) {}

        bool operator()(T v)
        {
            return v == _v;
        }

    private:
        T _v;
    };

    shared_ptr<TransportServer> TransportServer::create(string km_urn, string transport_key)
    {
        ThreadLock<decltype(factories_mutex)> l(factories_mutex);
        Keymaster km(km_urn);
        vector<TransportServer::factory_sig> facts;
        vector<string>::const_iterator i;
        vector<string> transports = km.get_as<vector<string> >(transport_key + ".Specified");

        l.lock();

        for (i = transports.begin(); i != transports.end(); ++i)
        {
            if (factories.find(*i) != factories.end())
            {
                facts.push_back(factories[*i]);
            }
        }

        if (transports.size() != facts.size())
        {
            throw(CreationError("Not all transports supported.", transports));
        }

        if (!all_of(facts.begin(), facts.end(), same_as<factory_sig>(facts.front())))
        {
            throw(CreationError("Some transports have different factories.", transports));
        }

        // They're all there, they are all the same. Create and return the
        // correct class object:

        factory_sig fn = facts.front();
        shared_ptr<TransportServer> ret_val(fn(km_urn, transport_key));
        return ret_val;
    }

    TransportServer::TransportServer(string keymaster_url, string key)
        : _km_url(keymaster_url),
          _transport_key(key)
    {
    }

    TransportServer::~TransportServer()
    {
    }

// These methods are meant to be abstract. However, we may
// find some common functionality. For now we just emit
// an error message.
    bool TransportServer::_register_urn(vector<string> urns)
    {
        cerr << "abstract method " << __func__ << " called" << endl;
        return false;
    }

    bool TransportServer::_unregister_urn()
    {
        cerr << "abstract method " << __func__ << " called" << endl;
        return false;
    }

// This will probably replace register_urn above
    bool TransportServer::_bind(vector<string> urns)
    {
        cerr << "abstract method " << __func__ << " called" << endl;
        return false;
    }

    bool TransportServer::_publish(string key, const void *data, size_t size_of_data)
    {
        cerr << "abstract method " << __func__ << " called" << endl;
        return false;
    }

    bool TransportServer::_publish(string key, string data)
    {
        cerr << "abstract method " << __func__ << " called" << endl;
        return false;
    }

/**********************************************************************
 * Transport Client
 **********************************************************************/

/**
 * Returns a shared_ptr to a TransportClient, creating a TransportClient
 * first if one does not exist for the keys given.
 *
 * @param urn: The fully formed URL to the data source, ready to use
 * to connect to that source.
 *
 * @return A shared_ptr to the transport object that was requested.
 *
 */

    shared_ptr<TransportClient> TransportClient::get_transport(string urn)
    {
        ThreadLock<decltype(transports)> l(transports);
        client_map_t::iterator cmi;

        l.lock();

        if ((cmi = transports.find(urn)) == transports.end())
        {
            transports[urn] = shared_ptr<TransportClient>(TransportClient::create(urn));
        }

        return transports[urn];
    }

/**
 * Manages the static transport map. The lifetime of a TransportClient
 * is determined by how many clients it has. If it has no more
 * clients, it gets deleted. The transport map uses shared_ptr to make
 * this determination. Every time a DataSink disconnects from a
 * TransportClient it resets its shared_ptr, and calls this
 * function. This function then checks to see if the shared_ptr in the
 * map is unique, that is, there are no other shared_ptrs sharing this
 * object. If so, it resets the shared pointer, and removes the entry
 * in the map.
 *
 * @param urn: The fully formed URN that the TransportClient uses to
 * connect to the TransportServer. Also the key to the transport map.
 *
 */

    void TransportClient::release_transport(string urn)
    {
        ThreadLock<decltype(transports)> l(transports);
        client_map_t::iterator cmi;

        l.lock();

        if ((cmi = transports.find(urn)) != transports.end())
        {
            if (cmi->second.unique())
            {
                cmi->second.reset();
            }

            transports.erase(cmi);
        }
    }

/**
 * Creates a TransportClient and returns it in a shared pointer.
 *
 * @param urn: The fully formed URN to the data sink.
 *
 * @return A std::shared_ptr<TransportClient> pointing to the
 * constructed TransportClient.
 *
 */

    shared_ptr<TransportClient> TransportClient::create(std::string urn)
    {
        ThreadLock<Mutex> l(factories_mutex);
        vector<string> parts;
        boost::split(parts, urn, boost::is_any_of(":"));
        
        if (!parts.empty())
        {
            factory_map_t::iterator i;
            
            l.lock();
            
            if ((i = factories.find(parts[0])) != factories.end())
            {
                factory_sig ff = i->second;
                return shared_ptr<TransportClient>(ff(urn));
            }

            throw TransportClient::CreationError("No known factory for " + parts[0]);
        }

        throw TransportClient::CreationError("Malformed URN " + urn);
    }


    TransportClient::TransportClient(string urn)
        : _urn(urn)
    {
    }
    
    TransportClient::~TransportClient()
    {
        disconnect();
    }
    
    bool TransportClient::_connect()
    {
        return false;
    }
    
    bool TransportClient::_disconnect()
    {
        return false;
    }
    
    bool TransportClient::_subscribe(string key, DataCallbackBase *cb)
    {
        return false;
    }
    
    bool TransportClient::_unsubscribe(string key)
    {
        return false;
    }


}

