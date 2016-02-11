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
 * Manages the static transport map. The lifetime of a TransportServer
 * is determined by how many clients it has. If it has no more
 * clients, it gets deleted. The transport map uses shared_ptr to make
 * this determination. Every time a DataSink disconnects from a
 * TransportServer it resets its shared_ptr, and calls this
 * function. This function then checks to see if the shared_ptr in the
 * map is unique, that is, there are no other shared_ptrs sharing this
 * object. If so, it resets the shared pointer, and removes the entry
 * in the map.
 *
 * @param urn: The fully formed URN that the TransportServer uses to
 * connect to the TransportServer. Also the key to the transport map.
 *
 */

    void TransportServer::release_transport(string component_name,
                                            string transport_name)
    {
        ThreadLock<decltype(transports)> l(transports);
        component_map_t::iterator cmi;
        transport_map_t::iterator tmi;

        l.lock();

        if ((cmi = transports.find(component_name)) != transports.end())
        {
            if ((tmi = cmi->second.find(transport_name)) != cmi->second.end())
            {
                // found! If no one is holding a pointer to this,
                // delete it and remove the entry.
                if (tmi->second.unique())
                {
                    tmi->second.reset();
                }

                cmi->second.erase(tmi);
            }
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

    std::map<std::string, data_description::types> data_description::typenames_to_types =
    {
        {"int8_t", data_description::INT8_T},
        {"uint8_t", data_description::UINT8_T},
        {"int16_t", data_description::INT16_T},
        {"uint16_t", data_description::UINT16_T},
        {"int32_t", data_description::INT32_T},
        {"uint32_t", data_description::UINT32_T},
        {"int64_t", data_description::INT64_T},
        {"uint64_t", data_description::UINT64_T},
        {"char", data_description::CHAR},
        {"unsigned char", data_description::UNSIGNED_CHAR},
        {"short", data_description::SHORT},
        {"unsigned short", data_description::UNSIGNED_SHORT},
        {"int", data_description::INT},
        {"unsigned int", data_description::UNSIGNED_INT},
        {"long", data_description::LONG},
        {"unsigned long", data_description::UNSIGNED_LONG},
        {"bool", data_description::BOOL},
        {"float", data_description::FLOAT},
        {"double", data_description::DOUBLE},
        {"long double", data_description::LONG_DOUBLE}
    };

    std::vector<size_t> data_description::type_info =
    {
        sizeof(int8_t),
        sizeof(uint8_t),
        sizeof(int16_t),
        sizeof(uint16_t),
        sizeof(int32_t),
        sizeof(uint32_t),
        sizeof(int64_t),
        sizeof(uint64_t),
        sizeof(char),
        sizeof(unsigned char),
        sizeof(short),
        sizeof(unsigned short),
        sizeof(int),
        sizeof(unsigned int),
        sizeof(long),
        sizeof(unsigned long),
        sizeof(bool),
        sizeof(float),
        sizeof(double),
        sizeof(long double)
    };


/**
 * Constructor to the data_description nested structure, which
 * contains a description of the data to be output for a source.
 *
 */

    data_description::data_description()
    {
    }

    data_description::data_description(YAML::Node fields)
    {
        if (fields.IsSequence())
        {
            vector<vector<string> > vs = fields.as<vector<vector<string> > >();

            for (vector<vector<string> >::iterator k = vs.begin(); k != vs.end(); ++k)
            {
                add_field(*k);
            }
        }
        else if (fields.IsMap())
        {
            map<string, vector<string> > vs = fields.as<map<string, vector<string> > >();

            for (int i = 0; i < vs.size(); ++i)
            {
                std::string s = std::to_string(i);
                auto p = vs.find(s);
                if (p != vs.end())
                {
                    add_field(vs[s]);
                }
                else
                {
                    ostringstream msg;
                    msg << "Unable to find entry " << s << " in parsing data description" << endl;
                    msg << "YAML input was: " << fields << endl;
                    throw MatrixException("data_description::data_description()",
                                          msg.str());
                }
            }
        }
        else
        {
            ostringstream msg;
            msg << "Unable to convert YAML input " << fields << "Neither vector or map.";
            throw MatrixException("TestDataGenerator::_read_source_information()",
                                  msg.str());
        }
    }

/**
 * Adds a field description to the fields list in the data_description object. Field
 * descriptions consist of a vector<string> of the form
 *
 *      [field_name, field_type, initial_val]
 *
 * The fields list describes the data structure.
 *
 * @param f: the vector containing the description for the field.
 *
 */

    void data_description::add_field(std::vector<std::string> &f)
    {
        data_description::data_field df;

        df.name = f[0];
        df.type = typenames_to_types[f[1]];
        df.elements = convert<size_t>(f[2]);

        if (f.size() > 3 && f[3] == "nolog")
        {
            df.skip = true;
        }
        else
        {
            df.skip = false;
        }
        fields.push_back(df);
    }
    
/**
 * Computes the size of the total buffer needed for the data source,
 * and the offset of the various fields. gcc in the x86_64
 * architecture stores structures in memory as a multiple of the
 * largest type of field in the structure. Thus, if the largest
 * element is an int, the size will be a multiple of 4; if it is a
 * long, of 8; etc. All elements must be stored on boundaries that
 * respect their size: int16_t on two-byte boundaries, int on 4-byte
 * boundaries, etc. Padding is added to make this work.
 *
 * case of long or double as largest field:
 *
 *     ---------------|---------------
 *               8 - byte value
 *     ---------------|---------------
 *       4-byte val   | 4-byte val
 *     ---------------|---------------
 *       4-byte val   |  padding (pd)
 *     ---------------|---------------
 *               8 - byte value
 *     ---------------|---------------
 *     1BV|1BV|  2BV  | 4-byte val
 *     ---------------|---------------
 *     1BV|    pd     | 4-byte val
 *
 *              etc...
 *
 * case of int or float being largest field:
 *
 *     ---------------
 *      4-byte value
 *     ---------------
 *      2BV   |1BV|pd
 *     ---------------
 *      1BV|pd| 2BV
 *     ---------------
 *      4-byte value
 *     ---------------
 *      2BV   | pd
 *
 *        etc...
 *
 * Structures that contain only one field are the size of that field
 * without any padding, since that one element is the largest element
 * type. So for a struct foo_t {int16_t i16;};, sizeof(foo_t) would be
 * 2.
 *
 * As it is computing the size this function also saves the offsets
 * into the various 'data_field' structures so that the data may be
 * properly accessed later.
 *
 * @return A size_t which is the size that the GenericBuffer should be
 * set to in order to contain the data properly.
 *
 */

    size_t data_description::size()
    {
        // storage element size, offset in element, number of elements
        // used.
        size_t s_elem_size, offset(0), s_elems(1);

        // find largest element in structure.
        std::list<data_field>::iterator i =
            max_element(fields.begin(), fields.end(),
                        [this](data_field &i, data_field &j)
                        {
                            return type_info[i.type] < type_info[j.type];
                        });
        s_elem_size = type_info[i->type];

        // compute the offset and multiples of largest element
        for (list<data_field>::iterator i = fields.begin(); i != fields.end(); ++i)
        {
            size_t s(type_info[i->type]);
            offset += offset % s;

            if (s_elem_size - offset < s)
            {
                offset = 0;
                s_elems++;
            }

            i->offset = s_elem_size * (s_elems - 1) + offset;
            offset += s;
        }

        return s_elem_size * s_elems;
    }

}
