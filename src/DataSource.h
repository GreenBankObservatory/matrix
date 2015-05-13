/*******************************************************************
 *  DataSource.h - A template class that provides an interface to
 *  publish data
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

#if !defined(_DATA_SOURCE_H_)
#define _DATA_SOURCE_H_

#include "Keymaster.h"

namespace matrix
{

/**
 * \class DataSource
 *
 * A data source provides the input end of a data's path to the
 * consumer, or sink. The interface is the same regardless of the data
 * or the underlying Transport. Data sources are specified in the YAML
 * configuration using the 'Source' keyword as follows:
 *
 *     nettask:
 *     ...
 *     ...
 *     Source:
 *       Data: A
 *       Log: A
 *       Foo: A
 *
 * Here 'Source' is a dictionary of 3 associations. The key is the
 * source name. The value is itself a key to the transport that this
 * source will need to use to send data. So this denotes 3 DataSources,
 * all sharing the same transport 'A'. An example use would be:
 *
 *     auto log = new DataSource("Log", km_urn, component_key);
 *     log->put(logmsg);
 *
 * The constructor takes three arguments:
 *
 *  - the data name
 *  - the keymaster urn
 *  - the component key
 *
 * These are all it needs to set itself up. Using 'km_urn', the
 * constructor can connect to the Keymaster and obtain the component
 * node specified by 'component_key'. Using this YAML node it sees that
 * there is indeed a data source "Log", and that it uses transport
 * "A". Using this information it can create and/or obtain a reference
 * to the correct transport. From this point on, 'log->put(data)' will
 * then translate to a 't->put("log", data)', where 't' is the correct
 * transport specified in the configuration.
 *
 */

    template <typename T>
    class DataSource
    {
    public:
        DataSource(std::string km_urn, std::string component_name, std::string data_name);
        ~DataSource() throw() {};

        bool publish(T &);

    private:
        std::string _km_urn;
        std::string _component_name;
        std::string _data_name;
        std::string _key;
        std::shared_ptr<TransportServer> _ts;
    };

/**
 * Creates a DataSource, given a data name, a keymaster urn, and a
 * component name.
 *
 * @param name: The name of the data source.
 *
 * @param km_urn: The keymaster urn
 *
 * @param component_name: The name of the component.
 *
 */

    template <typename T>
    DataSource<T>::DataSource(std::string km_urn, std::string component_name, std::string data_name)
        :
          _km_urn(km_urn),
          _component_name(component_name),
          _data_name(data_name),
          _key(component_name + "." + data_name)
    {
        Keymaster km(km_urn);
        // obtain the transport name associated with this data source and
        // get a pointer to that transport
        std::string transport_name = km.get_as<std::string>("components."
                                                            + component_name
                                                            + ".Sources."
                                                            + data_name);
        _ts = TransportServer::get_transport(km_urn, component_name, transport_name);
    }

/**
 * Puts a value of type 'T' to the data source. 'T' must be a
 * contiguous type: a POD, or struct of PODS, or an array of such.
 *
 * @param data: The data to send.
 *
 * @return true if the put succeeds, false otherwise.
 *
 */

    template <typename T>
    bool DataSource<T>::publish(T &val)
    {
        return _ts->publish(_key, &val, sizeof val);
    }

/**
 * Specialization for std::string version.
 *
 * @param val: A std::string acting as a buffer.
 *
 * @return true if the put succeeds, false otherwise.
 *
 */

    template <>
    bool DataSource<std::string>::publish(std::string &val)
    {
        return _ts->publish(_key, val);
    }
}

#endif
