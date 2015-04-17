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

#include "Mutex.h"

#include <string>
#include <memory>
#include <exception>
#include <yaml-cpp/yaml.h>
#include <boost/algorithm/string.hpp>

/// A strategy pattern class for the source-->sink data interface data between Components.
/// The intent is to provide a transport independent interface for publishing
/// data using various 0MQ and/or real-time extensions.

class TransportServer;

/**
 * These classes will provide a couple of layers of abstration to data
 * connections.
 *
 * At the top level are data sources and sinks. In the simplest terms, a
 * data sink represents one data stream (one type or named data
 * entity). A sink of 'Foo' connects to a source of 'Foo'. Sources and
 * sinks don't care, from a programmatic standpoint, how the data gets
 * to where it is going. Sinks send data, sources get data, how doesn't
 * matter at this level.
 *
 * At a lower level are the Transport classes. Transports provide the
 * 'how' mechanism to conduct data from point A (the server) to point B
 * (the client or clients). This may be a semaphore queue (such as
 * tsemfifo), ZMQ sockets, shared memory, real time queues, etc. One
 * transport may also be able to serve several data sources under the
 * hood. It is this abstraction that may be extended to provide
 * different mechanisms to get data from A to B.
 */

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

class DataSource
{
public:
    DataSource(std::string name, std::string km_urn, std::string component_name);
    ~DataSource() throw() {};

    bool publish(std::string);
    bool publish(const void *, size_t);

private:
    std::string _name;
    std::string _km_urn;
    std::string _component_name;
    std::shared_ptr<TransportServer> _ts;

    // this static member holds all the constructed transport servers
    // available. It is keyed by the component and transport
    // names. Because it may be accessed from any number of threads and
    // components it is also Protected.
    typedef std::map<std::string, std::shared_ptr<TransportServer> > transport_map_t;
    typedef Protected<std::map<std::string, transport_map_t> > component_map_t;

    static component_map_t transports;
    // this static function will return a shared pointer to a
    // constructed transport, constructing one if it doesn't already exist.
    static std::shared_ptr<TransportServer> get_transport(std::string km_urn,
                                                          std::string component_name,
                                                          std::string transport_name);
};

/**
 * \class TransportServer
 *
 * The Transport server takes data given to it by the DataSource and
 * transmits it to the recipient via some sort of transport
 * technology. The details of the underlying transport are private and
 * not known to the DataSource. Several implementations may be used:
 * ZMQ, shared memory, semafore FIFOS, etc.

 * Transports are specified in the YAML configuration using the
 * 'Transport' keyword inside of a component specification as follows:
 *
 *     nettask:
 *       Transports:
 *         A:
 *           Specified: [inproc, tcp]
 *           AsConfigured: [inproc://slizlieollwd, tcp://ajax.gb.nrao.edu:32553]
 *       ...
 *       ...
 *
 * Here the component, 'nettask', has 1 transport service, 'A'. It is
 * specified to support the 'inproc' and 'tcp' transports via the
 * 'Specified' keyword. Note that the value of this key is a vector;
 * thus more than one transport may be specified, but they must be
 * compatible. Here, 'inproc' and 'tcp' transports are both supported by
 * 0MQ, which also supports the 'ipc' transport. Thus if this list
 * contains any combination of the three a ZMQ based TransportServer
 * will be created. DataSource 'Log' will use this Transport because the
 * key 'Log' has as its value the key 'A', which specifies this
 * transport.
 *
 * When the transport is constructed it will turn the 'Specified' list
 * of transports into an 'AsConfigured' list of URLs that may be used by
 * clients to access this service. (Clients will transparently access
 * these via the TransportClient class & Keymaster.)
 *
 * Note here that in transforming the transport specification into URLs
 * the software generates random, temporary URLs. IPC and INPROC get a
 * string of random alphanumeric characters, and TCP gets turned into a
 * tcp url that contains the canonical host name and an ephemeral port
 * randomly chosen. This can be controlled by providing partial or full
 * URLs in place of the transport specifier:
 *
 *      nettask:
 *        Transports:
 *          A:
 *            Specified: [inproc://matrix.nettask.XXXXX, tcp://*]
 *            AsConfigured: [inproc://matrix.nettask.a4sLv, tcp://ajax.gb.nrao.edu:52016]
 *
 * The 'AsSpecified' values will be identical for inproc and ipc, with
 * the exception that any 'XXXXX' values at the end of the URL will be
 * replaced by a string of random alphanumeric characters of the same
 * length; and for TCP the canonical host name will replace the '*' (as
 * it is needed by potential clients) and a randomly assigned port
 * number from the ephemeral port range will be provided, if no port is
 * given.
 *
 * NOTE ON EXTENDING THIS CLASS: Users who wish to create their own
 * derivations of TransportServer, as in the example above, need only create
 * the new class with a static 'factory' method that has the signature
 * `TransportServer *(*)(std::string, std::string)`. The static
 * 'TransportServer::factories' map must then be updated with this new
 * factory method, with the supported transport as keys. Note that the
 * transport names must be unique! If a standard name, or one defined
 * earlier, is reused, the prefious factory will be unreachable.
 *
 *     // 1) declare the new TransportServer class
 *     class MyTransportServer : public TransportServer
 *     {
 *     public:
 *         MyTransportServer(std::string, std::string);
 *         ~MyTransportServer();
 *         bool publish(std::string, const void *, size_t);
 *         bool publish(std::string, std::string);
 *
 *         static TransportServer *factory(std::string, std::string);
 *     };
 *
 *     // 2) implement the new class
 *            ...
 *
 *     // 3) Add the new factory
 *     vector<string> transports = {'my_transport'};
 *     TransportServer::add_factory(transports, TransportServer *(*)(string, string));
 *
 */

class TransportServer
{
public:
    TransportServer(std::string keymaster_url, std::string key);
    virtual ~TransportServer();

    bool bind(std::vector<std::string> urns);
    bool publish(std::string key, const void *data, size_t size_of_data);
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
            msg = std::string("Cannot create TransportServer for transports "
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

    typedef TransportServer *(*factory_sig)(std::string, std::string);

    // Manage the factories map:
    static void add_factory(std::vector<std::string>, factory_sig);

    // 'create()' will map from the transport(s) it finds at
    // 'transport_key' to the correct TransportServer type
    static std::shared_ptr<TransportServer> create(std::string km_urn, std::string transport_key);

protected:

    virtual bool _bind(std::vector<std::string> urns);
    virtual bool _publish(std::string key, const void *data, size_t size_of_data);
    virtual bool _publish(std::string key, std::string data);

    bool _register_urn(std::vector<std::string> urns);
    bool _unregister_urn();

    std::string _km_url;
    std::string _transport_key;

private:
    // the factories map. For types that handle multiple transports
    // there will be more than one entry for that factory. This is of
    // little consequence.
    typedef std::map<std::string, factory_sig> factory_map_t;
    static factory_map_t factories;
    static Mutex factories_mutex;
};

inline bool TransportServer::bind(std::vector<std::string> urns)
{
    return _bind(urns);
}

inline bool TransportServer::publish(std::string key, const void *data, size_t size_of_data)
{
    return _publish(key, data, size_of_data);
}

inline bool TransportServer::publish(std::string key, std::string data)
{
    return _publish(key, data);
}



/// A strategy based class for the Component data sinks.

/**
DataSinks:
----------
After the Component TransportServers are constructed, the Controller may issue
a "prepare" event to instruct the Components to connect their DataSinks
to the now bound TransportServers. Each DataSink then searches the keymaster
for the bound urn for the TransportServer it needs. Recall that for DataSinks, each
component is provided a path of the TransportServer, and a transport to
use. An example of the keys for the TransportServer might be (repeating a
portion of the the example from the TransportServer page) the key/values for the
bound TransportServer 'A' would be:

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

The myLogger Component input_data DataSink wanting to get data from the TransportServer noted above
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
