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

#ifndef DataInterface_h
#define DataInterface_h

#include <string>
#include <memory>
#include <exception>
#include <yaml-cpp/yaml.h>
#include <boost/algorithm/string.hpp>

/// A strategy pattern class for the source-->sink data interface data between Components.
/// The intent is to provide a transport independent interface for publishing
/// data using various 0MQ and/or real-time extensions.

/**
DataSources:
------------
A Component data source specification which specifies a DataSource
binding (using the derived ZMQDataSource) to the tcp and inproc transport
transport will contain keymaster keys from the configuration file:

      components:
         myProducer:
            type: DataProducer
            source:
                A: transport [tcp, inproc, rtinproc]

Resulting in the following keys in the keymaster database:

   * components.myProducer.source.A.transport[0] = "tcp"
   * components.myProducer.source.A.transport[1] = "inproc"
   * components.myProducer.source.A.transport[2] = "rtinproc"

As the Component creates and binds the DataSource to the various transports
it adds urn nodes which detail the run-time bound urns. Continuing with
the example above, the Component would add to the keymaster database:

   * components.myProducer.source.A.urn[0] = "tcp://*.9925"
   * components.myProducer.source.A.urn[1] = "inproc://myLogger_A"
   * components.myProducer.source.A.urn[2] = "rtinproc://0x26A4000"

Thus indicating to DataSources exactly how to bind to the desired
transport. DataSinks, given the path to the DataSource and which
transport to use can then easily locate the (optionally) dynamically
assigned tranport port number or address.

***/

/**
 * \class DataSource
 *
 * This is the base class for a data source. DataSource uses a static
 * method to create the appropriate DataSource type given a vector of
 * transports. Because the code that creates an object need not know
 * about the specific implementation of DataSource, this allows base
 * classes, such as the Component base class, to create even
 * user-derived DataSources.  So, given this YAML configuration:
 *
 *     components:
 *       nettask:
 *         Transports:
 *           A:
 *             Specified: [my_transport]
 *
 * The configuration specifies a 'my_transport' transport. The following
 * code would automatically generate a MyDataSource object, without that
 * typename appearing anywhere in the calling code:
 *
 *     std::shared_ptr<DataSource> ds;
 *     std::string keymaster = "inproc://matrix.keymaster";
 *     std::string key = "components.nettask.Transports.A";
 *     ds = DataSource::create(keymaster, key);
 *
 * In this example, 'create(...)' takes a keymaster urn and a key. It
 * creates a Keymaster client, and fetches the value of 'key'. Looking
 * for 'Specified', it finds the vector [my_transport]. Using this
 * vector it looks up the correct factory method, and creates the
 * correct object.
 *
 * NOTE ON EXTENDING THIS CLASS: Users who wish to create their own
 * derivations of DataSource, as in the example above, need only create
 * the new class with a static 'factory' method that has the signature
 * `DataSource *(*)(std::string, std::string)`. The static
 * 'DataSource::factories' map must then be updated with this new
 * factory method, with the supported transport as keys. Note that the
 * transport names must be unique! If a standard name, or one defined
 * earlier, is reused, the prefious factory will be unreachable.
 *
 *     // 1) declare the new DataSource class
 *     class MyDataSource : public DataSource
 *     {
 *     public:
 *         MyDataSource(std::string, std::string);
 *         ~MyDataSource();
 *         bool register_urn(std::string);
 *         bool unregister_urn(std::string);
 *         bool bind(std::vector<std::string>);
 *         bool publish(std::string, const void *, size_t);
 *         bool publish(std::string, std::string);
 *
 *         static DataSource *factory(std::string, std::string);
 *     };
 *
 *     // 2) implement the new class
 *            ...
 *
 *     // 3) Add the new factory
 *     vector<string> transports = {'my_transport'};
 *     DataSource::add_factory(transports, DataSource *(*)(string, string));
 *
 */

class DataSource
{
public:
    DataSource(std::string keymaster_url, std::string key);
    virtual ~DataSource();

    bool bind(std::vector<std::string> urns);
    bool publish(std::string key, void *data, size_t size_of_data);
    bool publish(std::string key, std::string data);

    // exception type for this class.
    class CreationError : public std::exception
    {
        std::string msg;

    public:

        CreationError(std::string err_msg,
                      std::vector<std::string> t = std::vector<std::string>())
        {
            std::string transports = boost::algorithm::join(t, ", ");
            msg = std::string("Cannot create DataSource for transports "
                              + transports + ": " + err_msg);
        }

        ~CreationError() throw()
        {
        }

        const char *what() const throw()
        {
            return msg.c_str();
        }
    };

    typedef DataSource *(*factory_sig)(std::string, std::string);

    // Manage the factories map:
    static void add_factory(std::vector<std::string>, factory_sig);

    // 'create()' will map from the transport(s) it finds at
    // 'transport_key' to the correct DataSource type
    static std::shared_ptr<DataSource> create(std::string km_urn, std::string transport_key);

protected:

    virtual bool _bind(std::vector<std::string> urns);
    virtual bool _publish(std::string key, void *data, size_t size_of_data);
    virtual bool _publish(std::string key, std::string data);

    bool _register_urn(std::vector<std::string> urns);
    bool _unregister_urn();

    std::string _km_url;
    std::string _transport_key;

private:
    // the factories map. For types that handle multiple transports
    // there will be more than one entry for that factory. This is of
    // little consequence.
    static std::map<std::string, factory_sig> factories;
};

inline bool DataSource::bind(std::vector<std::string> urns)
{
    return _bind(urns);
}

inline bool DataSource::publish(std::string key, void *data, size_t size_of_data)
{
    return _publish(key, data, size_of_data);
}

inline bool DataSource::publish(std::string key, std::string data)
{
    return _publish(key, data);
}



/// A strategy based class for the Component data sinks.

/**
DataSinks:
----------
After the Component DataSources are constructed, the Controller may issue
a "prepare" event to instruct the Components to connect their DataSinks
to the now bound DataSources. Each DataSink then searches the keymaster
for the bound urn for the DataSource it needs. Recall that for DataSinks, each
component is provided a path of the DataSource, and a transport to
use. An example of the keys for the DataSource might be (repeating a
portion of the the example from the DataSource page) the key/values for the
bound DataSource 'A' would be:

   * components.myProducer.source.A.urn[0] = "tcp://*.9925"
   * components.myProducer.source.A.urn[1] = "inproc://myLogger_A"
   * components.myProducer.source.A.urn[2] = "rtinproc://0x26A4000"

The configuration file for various Components includes a *connections.mode*
section, which has the syntax:

   * [<Component name>, <source name>, <Component name>, <sink name>, transport]

So provided a hypothetical entry for the connections section:

        connections:
            VEGAS_1:
                - [myProducer, A, myLogger, input_data, tcp]

The myLogger Component input_data DataSink wanting to get data from the DataSource noted above
would generate and query for the key:

   * components.myProducer.sink.A.urn

Then it would use the connections transport field to search for a binding of the same type.
In this case it would select:

   * components.myProducer.source.A.urn[0] = "tcp://*:9925"

The DataSink would then use the ZMQDataSink to connect to the tcp transport at the localhost at port 9925.



***/

class DataSink
{
public:
    DataSink();
    virtual ~DataSink();

    bool connect(std::string urn_from_keymaster)
    {
        return _connect(urn_from_keymaster);
    }

    bool subscribe(std::string urn_from_keymaster)
    {
        return _subscribe(urn_from_keymaster);
    }
    bool unsubscribe(std::string urn_from_keymaster)
    {
        return _unsubscribe(urn_from_keymaster);
    }
    bool get(void *v, size_t &size_of_data)
    {
        return _get(v, size_of_data);
    }
    bool get(std::string &data)
    {
        return _get(data);
    }

protected:
    virtual bool _connect(std::string urn_from_keymaster);
    virtual bool _subscribe(std::string urn_from_keymaster);
    virtual bool _unsubscribe(std::string urn_from_keymaster);
    virtual bool _get(void *v, size_t &size_of_data);
    virtual bool _get(std::string &data);

};


#endif
