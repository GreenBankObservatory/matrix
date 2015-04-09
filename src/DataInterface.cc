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

#include <iostream>

using namespace std;

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

void DataSource::add_factory(vector<string> transports, DataSource::factory_sig factory)
{
    vector<string>::const_iterator i;

    for (i = transports.begin(); i != transports.end(); ++i)
    {
        factories[*i] = factory;
    }
}

/**
 * Creates the correct type of DataSource.
 *
 * This function is given the urn for the keymaster, and a transport
 * key. Using this key it obtains the transport information, which is
 * then used to create the correct DataSource object for that transport.
 *
 * @param km_urn: the URN for the keymaster
 *
 * @return A shared_ptr<DataSource> for the created object.
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

shared_ptr<DataSource> DataSource::create(string km_urn, string transport_key)
{
    Keymaster km(km_urn);
    vector<DataSource::factory_sig> facts;
    vector<string>::const_iterator i;

    vector<string> transports = km.get_as<vector<string> >(transport_key + ".Specified");

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

    return shared_ptr<DataSource>((*facts.front())(km_urn, transport_key));
}

DataSource::DataSource(string keymaster_url, string key)
    : _km_url(keymaster_url),
      _transport_key(key)
{
}

DataSource::~DataSource()
{
}

// These methods are meant to be abstract. However, we may
// find some common functionality. For now we just emit
// an error message.
bool DataSource::_register_urn(vector<string> urns)
{
    cerr << "abstract method " << __func__ << " called" << endl;
    return false;
}

bool DataSource::_unregister_urn()
{
    cerr << "abstract method " << __func__ << " called" << endl;
    return false;
}

// This will probably replace register_urn above
bool DataSource::_bind(vector<string> urns)
{
    cerr << "abstract method " << __func__ << " called" << endl;
    return false;
}

bool DataSource::_publish(string key, void *data, size_t size_of_data)
{
    cerr << "abstract method " << __func__ << " called" << endl;
    return false;
}

bool DataSource::_publish(string key, string data)
{
    cerr << "abstract method " << __func__ << " called" << endl;
    return false;
}


DataSink::DataSink()
{
}

DataSink::~DataSink()
{
}

bool DataSink::_connect(string urn_from_keymaster)
{
    cerr << "abstract method " << __func__ << " called" << endl;
    return false;
}

bool DataSink::_subscribe(string urn_from_keymaster)
{
    cerr << "abstract method " << __func__ << " called" << endl;
    return false;
}

bool DataSink::_unsubscribe(string urn_from_keymaster)
{
    cerr << "abstract method " << __func__ << " called" << endl;
    return false;
}

bool DataSink::_get(void *v, size_t &size_of_data)
{
    cerr << "abstract method " << __func__ << " called" << endl;
    return false;
}

bool DataSink::_get(string &data)
{
    cerr << "abstract method " << __func__ << " called" << endl;
    return false;
}
