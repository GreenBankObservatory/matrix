/*******************************************************************
 *  DataInterface.h - * These classes provide abstrations to data
 *  connections.
 *
 *  At the top level are data sources and sinks. In the simplest
 *  terms, a data sink represents one data stream (one type or named
 *  data entity). A sink of 'Foo' connects to a source of
 *  'Foo'. Sources and sinks don't know how the data gets to where it
 *  is going. Sinks send data, sources get data, how doesn't matter at
 *  this level.
 *
 *  At a lower level are the Transport classes. Transports provide the
 *  'how' mechanism to conduct data from point A (the server) to point
 *  B (the client or clients). This may be a semaphore queue (such as
 *  tsemfifo), ZMQ sockets, shared memory, real time queues, etc. One
 *  transport may also be able to serve several data sources under the
 *  hood. It is this abstraction that may be extended to provide
 *  different mechanisms to get data from A to B.
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

#ifndef DataInterface_h
#define DataInterface_h

#include "matrix/Mutex.h"
#include "matrix/ThreadLock.h"

#include <string>
#include <memory>
#include <exception>
#include <yaml-cpp/yaml.h>
#include <boost/algorithm/string.hpp>

namespace matrix
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcomment"
/**
 * \class GenericBuffer
 *
 * This provides a buffer class for which a specialization of DataSink
 * exists to send _not the class itself_ but only the contents of
 * buffer attached to it over the line. This allows a
 * DataSource<GenericBuffer> to be dynamically configured to be the
 * data source for any sink, provided the buffer is set to the right
 * size and is structured appropriately. A (contrived) example:
 *
 *      struct foo
 *      {
 *          int a;
 *          int b;
 *          double c;
 *          int d;
 *      };
 *
 *      DataSource<GenericBuffer> src;
 *      DataSink<foo> sink;
 *      GenericBuffer buf;
 *      buf.resize(sizeof(foo));
 *      foo *fp = (foo *)buf.data();
 *      fp->a = 0x1234;
 *      fp->b = 0x2345;
 *      fp->c = 3.14159;
 *      fp->d = 0x3456;
 *      src.publish(buf);
 *
 *      // if 'sink' is connected to 'src', then:
 *      foo msg;
 *      sink.get(msg);
 *      cout << hex << msg.a
 *           << ", " << msg.b
 *           << ", " << dec << msg.c
 *           << ", " << hex << msg.d << endl;
 *      // output is "1234, 2345, 3.14159, 3456"
 *
 * So why do this? Why not just use a DataSource<foo> to begin with?
 * The answer to this is that now we can _dynamically_ create a data
 * source that will satisfy any DataSink. Now it satisfies
 * DataSink<foo>, but later in the life of the program the same
 * DataSource can publish data that will satisfy a DataSink<bar> for a
 * struct bar. Such a DataSource would allow a hypothetical component
 * to dynamically at run-time read a description of 'bar' somewhere
 * (from the Keymaster, for example), resize the GenericBuffer 'buf',
 * load it according to the description, and publish a 'bar'
 * compatible payload. This can be very useful during program
 * development, where a component is being developed and tested but
 * an upstream component is not yet available. A generic test
 * component as described above may be used in its place.
 *
 */
#pragma GCC diagnostic pop

    class GenericBuffer
    {
    public:
        GenericBuffer()
        {
        }

        GenericBuffer(const GenericBuffer &gb)
        {
            _copy(gb);
        }

        void resize(size_t size)
        {
            _buffer.resize(size);
        }

        size_t size() const
        {
            return _buffer.size();
        }

        unsigned char *data()
        {
            return _buffer.data();
        }

        const GenericBuffer &operator=(const GenericBuffer &rhs)
        {
            _copy(rhs);
            return *this;
        }

    private:
        void _copy(const GenericBuffer &gb)
        {
            if (_buffer.size() != gb._buffer.size())
            {
                _buffer.resize(gb._buffer.size());
            }

            memcpy((void *)_buffer.data(), (void *)gb._buffer.data(), _buffer.size());
        }

        std::vector<unsigned char> _buffer;
    };

    struct data_description
    {
        enum types
        {
            INT8_T,
            UINT8_T,
            INT16_T,
            UINT16_T,
            INT32_T,
            UINT32_T,
            INT64_T,
            UINT64_T,
            CHAR,
            UNSIGNED_CHAR,
            SHORT,
            UNSIGNED_SHORT,
            INT,
            UNSIGNED_INT,
            LONG,
            UNSIGNED_LONG,
            BOOL,
            FLOAT,
            DOUBLE,
            LONG_DOUBLE,
            TIME_T
        };

        struct data_field
        {
            std::string name;         // field name
            types type;               // field type
            size_t offset;            // offset into buffer
            size_t elements;          // 1 or more of these
            bool skip;                // flag to ignore field when logging
        };

        data_description();
        data_description(YAML::Node fields);
        void add_field(std::vector<std::string> &f);
        size_t size();

        double interval; // in seconds
        std::list<data_field> fields;

        static std::vector<size_t> type_info;
        static std::map<std::string, types> typenames_to_types;
    };

    template <typename T>
        T get_data_buffer_value(unsigned char *buf, size_t offset)
    {
        return *((T *)(buf + offset));
    }

    template <typename T>
        void set_data_buffer_value(unsigned char *buf, size_t offset, T val)
    {
        *((T *)(buf + offset)) = val;
    }

/**********************************************************************
 * Callback classes
 **********************************************************************/

/**
 * \class DataCallbackBase
 *
 * A virtual pure base callback class. A pointer to one of these is
 * given when subscribing for a keymaster value. When the value is
 * published, it is received by the Keymaster client object, which then
 * calls the provided pointer to an object of this type.
 *
 */

    struct DataCallbackBase
    {
        void operator()(std::string key, void *val, size_t sze) {_call(key, val, sze);}
        void exec(std::string key, void *val, size_t sze)       {_call(key, val, sze);}
    private:
        virtual void _call(std::string key, void *val, size_t szed) = 0;
    };

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcomment"
/**
 * \class DataMemberCB
 *
 * A subclassing of the base DataCallbackBase callback class that allows
 * a using class to use one of its methods as the callback.
 *
 * example:
 *
 *     class foo()
 *     {
 *     public:
 *         void bar(std::string, void *, size_t) {...}
 *
 *     private:
 *         DataMemberCB<foo> my_cb;
 *         DataSink *km;
 *     };
 *
 *     foo::foo()
 *       :
 *       my_cb(this, &foo::bar)
 *     {
 *         ...
 *         ds = new DataSink(data_name, km_url, component);
 *         ds->subscribe("Data", &my_cb); // assumes a data source named "Data"
 *     }
 *
 */
#pragma GCC diagnostic pop

    template <typename T>
    class DataMemberCB : public DataCallbackBase
    {
    public:
        typedef void (T::*ActionMethod)(std::string, void *, size_t);

        DataMemberCB(T *obj, ActionMethod cb) :
            _object(obj),
            _faction(cb)
        {
        }

    private:
        ///
        /// Invoke a call to the user provided callback
        ///
        void _call(std::string key, void *buf, size_t len)
        {
            if (_object && _faction)
            {
                (_object->*_faction)(key, buf, len);
            }
        }

        T  *_object;
        ActionMethod _faction;
    };

/**
 * \class GenericBufferHandler
 *
 * Base class to a callback functor enables actions to be defined by a
 * user of the GenericDataConsumer component, or other application.
 *
 */

    struct GenericBufferHandler
    {
        void operator()(YAML::Node dd, matrix::GenericBuffer &buf)
        {
            _call(dd, buf);
        }

        void exec(YAML::Node dd, matrix::GenericBuffer &buf)
        {
            _call(dd, buf);
        }

    private:
        virtual void _call(YAML::Node, matrix::GenericBuffer &)
        {
        }
    };

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcomment"
/**********************************************************************
 * transport_selection_strategy
 **********************************************************************/

/**
 * \class transport_selection_strategy
 *
 * A base class for a transport selection strategy class
 * hierarchy. This allows the caller to specify how to select from
 * various available transports if the data source uses more than one.
 *
 * Provided strategies are:
 *
 *   - select_only: Returns a URL to the data source if it is the only
 *     one available. If there are more than one, throws an
 *     exception. This will be used by default if no strategy is
 *     given.
 *
 *   - select_specified: Returns the URL for the specified transport
 *     if it exists, otherwise throws. The transport name is specified
 *     in the functor's constructor.
 *
 * Users could create their own based on this class. For example,
 * return one preferred transport, falling back to another if this
 * transport does not exist, etc.
 *
 */

    // class transport_selection_strategy
    // {
    // public:
    //     transport_selection_strategy(std::string km_url) : _km_url(km_url) {}

    //     virtual std::string operator() (std::string component,
    //                                     std::string data_name) = 0;
    // protected:
    //     std::string _km_url;
    // };

/**
 * \class select_only
 *
 * This functor, based on the `transport_selection_strategy` class,
 * picks the only transport available for a particular data source. If
 * there are more than one transports supported by this data source
 * this functor will throw an exception. If that is the case callers
 * should use the `select_specified` functor with the proper transport
 * specified.
 *
 */

    // class select_only : public transport_selection_strategy
    // {
    // public:
    //     select_only(std::string km_url): transport_selection_strategy(km_url) {}

    //     virtual std::string operator() (std::string component, std::string data_name);
    // };

/**
 * \class select_specified
 *
 * This functor returns the specified transport (as given to the
 * constructor) or it throws if this transport is not available.
 *
 */

    // class select_specified : public transport_selection_strategy
    // {
    // public:
    //     select_specified(std::string km_url, std::string transport)
    //         : transport_selection_strategy(km_url), _transport(transport)
    //     {
    //     }

    //     virtual std::string operator() (std::string component, std::string data_name);

    // private:
    //     std::string _transport;
    // };

/**********************************************************************
 * Transport Server
 **********************************************************************/

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
  *            Specified: [inproc://matrix.nettask.XXXXX, tcp://\*]
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
#pragma GCC diagnostic pop

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

        static void add_factory(std::vector<std::string>, factory_sig);
        static std::shared_ptr<TransportServer> get_transport(std::string km_urn,
                                                              std::string component_name,
                                                              std::string transport_name);
        static void release_transport(std::string component_name, std::string transport_name);

    protected:

        virtual bool _bind(std::vector<std::string> urns);
        virtual bool _publish(std::string key, const void *data, size_t size_of_data);
        virtual bool _publish(std::string key, std::string data);

        bool _register_urn(std::vector<std::string> urns);
        bool _unregister_urn();

        std::string _km_url;
        std::string _transport_key;

    private:

        static std::shared_ptr<TransportServer> create(std::string km_urn, std::string transport_key);

        typedef std::map<std::string, factory_sig> factory_map_t;
        static factory_map_t factories;
        static matrix::Mutex factories_mutex;

        typedef std::map<std::string, std::shared_ptr<TransportServer> > transport_map_t;
        typedef matrix::Protected<std::map<std::string, transport_map_t> > component_map_t;
        static component_map_t transports;
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

/**********************************************************************
 * Transport Client
 **********************************************************************/

/**
 * \class TransportClient
 *
 * Represents a client to a named transport on a named component. The
 * client may be shared by multiple DataSinks, provided they also need
 * access to the same transport on the same component.
 *
 * The client connects by looking up the component and transport on
 * the keymaster, which should return a URL. This URL is then used to
 * connect to the transport.
 *
 * The TransportClient object's lifetime is controlled by use, via
 * std::shared_ptr<>. When a DataSink requests a TransportClient to
 * make the connection to the DataSource, a new TransportClient of the
 * correct type will be created, and stored in a static map of
 * std::shared_ptr<TransportClients>. A copy of the shared pointer is
 * then returned to the DataSink. The next DataSink to require this
 * TransportClient will request it and get another shared_ptr<> copy
 * to it. When a DataSink disconnects or calls its destructor, it
 * `reset()`s its shared_ptr copy, and then calls its static
 * `release_transport()` with the component name and data name
 * keys. That function checks to see if the stored shared_ptr is
 * unique, and if so, it resets it, terminating the TransportClient.
 *
 */

    class TransportClient
    {
    public:
        TransportClient(std::string urn);
        virtual ~TransportClient();

        bool connect(std::string urn = "");
        bool disconnect();
        bool subscribe(std::string key, DataCallbackBase *cb);
        bool unsubscribe(std::string key);

        // exception type for this class.
        class CreationError : public std::exception
        {
            std::string msg;

        public:

            CreationError(std::string err_msg)
            {
                msg = std::string("Cannot create TransportClient for transport: "
                                  + err_msg);
            }

            ~CreationError() throw()
            {
            }

            const char *what() const throw()
            {
                return msg.c_str();
            }
        };

        typedef TransportClient *(*factory_sig)(std::string);

        static void add_factory(std::vector<std::string>, factory_sig);
        static std::shared_ptr<TransportClient> get_transport(std::string urn);
        static void release_transport(std::string urn);

    protected:

        virtual bool _connect();
        virtual bool _disconnect();
        virtual bool _subscribe(std::string key, DataCallbackBase *cb);
        virtual bool _unsubscribe(std::string key);

        std::string _urn;

    private:

        matrix::Mutex _shared_lock;

        static std::shared_ptr<TransportClient> create(std::string urn);

        typedef std::map<std::string, factory_sig> factory_map_t;
        static factory_map_t factories;
        static matrix::Mutex factories_mutex;

        typedef matrix::Protected<std::map<std::string, std::shared_ptr<TransportClient> > > client_map_t;
        static client_map_t transports;
    };

    inline bool TransportClient::connect(std::string urn)
    {
        matrix::ThreadLock<matrix::Mutex> l(_shared_lock);
        l.lock();

        // connect to new urn?
        if (!urn.empty())
        {
            _urn = urn;
        }

        return _connect();
    }

    inline bool TransportClient::disconnect()
    {
        matrix::ThreadLock<matrix::Mutex> l(_shared_lock);
        l.lock();
        return _disconnect();
    }

    inline bool TransportClient::subscribe(std::string key, DataCallbackBase *cb)
    {
        matrix::ThreadLock<matrix::Mutex> l(_shared_lock);
        l.lock();
        return _subscribe(key, cb);
    }

    inline bool TransportClient::unsubscribe(std::string key)
    {
        matrix::ThreadLock<matrix::Mutex> l(_shared_lock);
        l.lock();
        return _unsubscribe(key);
    }


} // namespace matrix

#include "DataSource.h"
#include "DataSink.h"

#endif
