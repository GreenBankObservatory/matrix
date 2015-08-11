/*******************************************************************
 *  DataSink.h - DataSink template class.
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

#if !defined (_DATA_SINK_H_)
#define _DATA_SINK_H_

#include "Time.h"
#include "tsemfifo.h"

#include <sstream>

/**
 * \class DataSink is the client data interface, and abstracts the
 * data from the transport mechanism.
 *
 * DataSink is associated with the desired data stream and because it
 * is a template it may be created as a type T of that data. It hides
 * the transport aspect of connecting to the data stream, receiving
 * it, etc.
 *
 * The operation of DataSink may be controlled to some extent by the
 * type chosen as T:
 *
 *  * If T is a simple, contiguous type that has a compiler-generated
 *    operator=() that operates by calling std::memset(), then no
 *    memory allocations occur during the operation of DataSink, only
 *    upon creation. However this means that the DataSink is rigid in
 *    the type received. For example, assuming a data source 'foo'
 *    from component 'foo_component' that provides doubles:
 *
 *          DataSink<double> ds(keymaster_urn);
 *          ds.connect('foo_component', 'foo')
 *          double val;
 *          ds.get(val); // waits for a 'val' from 'foo'
 *
 *  * If T is a reference counting pointer type like std::shared_ptr,
 *    then the buffer will be handed off from one pointer to the
 *    other, and claimed for destruction by the last surviving
 *    pointer. This involves memory allocation and deallocation.
 *
 *  * If T has a custom operator=(), then those semantics apply. For
 *    example, T could be a YAML::Node, or a protobuf, or MessagePack,
 *    etc., which may vary in length depending on what data is
 *    contained; or T may be something custom but which has fixed data
 *    sizes, the provided operator=() does memory copies, etc.
 *
 *  * For a DataSink that is not associated with a specific type, but
 *    with copy semantics (no memory allocation) and a fixed buffer
 *    size, use a DataSink<fixed_buffer<size> >.
 *
 *  * For a DataSink that is flexible as to the messages being
 *    received, but still not associated with any concrete type--for
 *    example, if only exchanging ASCII strings--at the cost of using
 *    std::strings operator =() semantics, in which a buffer of
 *    arbitrary length is handed off from one string to another, and
 *    which involves memory allocation, use a DataSink<std::string>.
 *
 * DataSink works by subscribing to a TransportClient with the desired
 * URL when `connect()` is called. The subscription key is
 * 'component_name.data_name'. When subscribing it passes a callback
 * functor to the TransportClient. This functor wraps the
 * '_data_handler()' member function, and receives the data directly
 * from the TransportClient and copies it into a tsemfifo<T>
 * buffer. DataSink may then 'get()' or 'try_get()' the data at the
 * tsemfifo head.
 *
 * A DataSink may disconnect and reconnect to a different data
 * source as many times as desired. To reconnect it just needs a new
 * component name and  data name. Note that the DataSink in question
 * must be compatible with the DataSource. For example, if it is a
 * DataSink<double> and both sources publish doubles.
 *
 */

#include "DataInterface.h"
#include "matrix_util.h"
#include <string>
#include <iostream>

namespace matrix
{
/**********************************************************************
 * transport_selection_strategy
 **********************************************************************/

/**
 * \class select_specified
 *
 * Returns the URL that coresponds to the requested transport. The
 * requested transport is provided as a template parameter for
 * `select_specified`. If a URL that uses that transport exists, it
 * will be returned. Otherwise a TransportClient::CreationError
 * exception will be thrown.
 *
 * example:
 *
 *      select_specified<"inproc"> ssp("inproc://matrix.keymaster");
 *      string url = ssp("frog", "song");
 *
 * Though as in the example this class may be used alone, it is
 * intended as a policy class for the DataSink template class:
 *
 *      DataSink<double, select_specified<"inproc"> > ds(km_urn);
 *      ds.connect("frog", "song"); // transport stuff hidden.
 *
 * The returned string `url` will be the inproc URL for the 'song'
 * data source on component 'frog'.
 *
 */

    class select_specified
    {
    public:
        select_specified(std::string km_urn, std::string transport)
            : _km_urn(km_urn),
              _transport(transport)
        {
        }

        std::string operator() (std::string component, std::string data_name)
        {
            Keymaster km(_km_urn);
            YAML::Node n = km.get("components." + component);
            std::string transport = n["Sources"][data_name].as<std::string>();
            std::vector<std::string> urls =
                n["Transports"][transport]["AsConfigured"].as<std::vector<std::string> >();
            std::vector<std::string>::iterator it =
                find_if(urls.begin(), urls.end(), mxutils::is_substring_in_p(_transport));

            if (it == urls.end()) // not found!
            {
                throw(TransportClient::CreationError(
                          "Transport " + _transport + " not configured by " + component + "." + data_name));
            }

            return *it;
        }

    private:
        std::string _km_urn;
        std::string _transport;
    };

/**
 * \class select_only
 *
 * This functor returns the only configured transport URL for the
 * named data source on the named component. If the data source has
 * configured no transports, or if it has configured more than 1,
 * throws a TransportClient::Creation Error. It is the defalut
 * selector for class DataSink:
 *
 *      DataSink<double> ds(km_urn); // will use 'select_only'
 *      ds.connect("frog", "song");  // transport stuff hidden.
 *
 */

    class select_only
    {
    public:
        select_only(std::string km_urn, std::string = "")
            : _km_urn(km_urn)
        {
        }

        std::string operator() (std::string component, std::string data_name)
        {
            Keymaster km(_km_urn);
            YAML::Node n = km.get("components." + component);
            std::string transport = n["Sources"][data_name].as<std::string>();
            std::vector<std::string> urls =
                n["Transports"][transport]["AsConfigured"].as<std::vector<std::string> >();

            if (urls.size() > 1)
            {
                throw(TransportClient::CreationError(
                          "Multiple transports with none specified for" + component + "." + data_name));
            }

            if (urls.empty())
            {
                throw(TransportClient::CreationError(
                          "No configured transports found for " + component + "." + data_name));
            }

            return urls[0];
        }

    private:
        std::string _km_urn;
    };

/**
 * \class DataSink
 *
 * DataSink is a template class that allows a client to connect to a
 * source, without having to set up the transport. This is done behind
 * the scenes, in accordance with data found in the Keymaster. The
 * only transport-related information that must be provided by the
 * caller is an election between potential transports, if the source
 * supports more than one.
 *
 * It is a template class that takes two parameters: typename 'T', the
 * type of data being received, and typename `urn_selector`, a policy
 * class that fetches one of possibly many URNs for the source. The
 * inclusion of this policy allows the caller to customize how the URN
 * is selected. Two basic policies are provided:
 *
 *   - select_only: Selects the URN if it is the only one available,
 *     otherwise throws. If this is used then no transport needs to be
 *     given to `connect()`
 *
 *   - select_specified: The default. Allows the transport to be
 *     explicitly named in the 'connect()' member function. If that
 *     transport does not exist, it throws.
 *
 * Other policies may be used.
 *
 * On connection the DataSink picks a transport client (which will be
 * created automatically if it doesn't already exist) by using the URN
 * obtained from the keymaster for the component, data, and transport
 * desired; and subscribes to it using a key which is in the format
 * "component_name.data_name". During the subscription process it also
 * provides the TransportServer with a callback functor that allows
 * the TransportServer to pass on data received with the subsription
 * key to a semaphore queue in the DataSink, from which it may be read
 * out by the DataSink caller.
 *
 * Example:
 *
 *   Assuming a `test.yaml` which includes the following:
 *
 *     components:
 *       moby_dick:
 *         Tranports:
 *           A:
 *             Specified: [inproc, ipc, tcp]
 *         Sources:
 *           lines: A
 *
 *     string km_urn("inproc://matrix.keymaster");
 *     KeymasterServer kms("test.yaml");
 *     kms.run();
 *
 *     DataSource<string> mbsource(km_urn, "moby_dick", "lines");
 *     DataSink<string, select_specified> mbsink(km_urn);
 *     mbsink.connect("moby_dick", "lines", "tcp");
 *     // avoid race between 'mbsink' thread being ready and publish below
 *     sleep(1);
 *     string msg = "Call me Ishmael.";
 *     mbsource.publish(msg);
 *     string rcv_msg;
 *     mbsink.get(rcv_msg);
 *     cout << "Received: " << rcv_msg << endl;
 *
 */

    template <typename T>
    void _data_handler(void *data, size_t sze, tsemfifo<T> &ringbuf)
    {
        if (sizeof(T) != sze)
        {
            std::ostringstream msg;
            msg << "size mismatch error. sizeof(T) == " << sizeof(T)
                << " and given data buffer size is " << sze;
            throw MatrixException("DataSink::_data_handler()", msg.str());
        }

        ringbuf.put(*(T *)data);
    }

    template <>
    inline void _data_handler<std::string>(void *data, size_t sze, tsemfifo<std::string> &ringbuf)
    {
        std::string val(sze, 0);
        std::memmove((char *)val.data(), data, sze);
        ringbuf.try_put(val);
    }

    template <typename T, typename U = select_specified>
    class DataSink
    {
    public:
        DataSink(std::string km_urn, size_t ringbuf_size = 10);
        ~DataSink() throw();

        void get(T &);
        bool try_get(T &);
        bool timed_get(T &, Time::Time_t);
        size_t items();

        void connect(std::string component_name, std::string data_name, std::string transport = "");
        void disconnect();

    private:

        // template <typename, typename> class params {};
        void _check_connected();

        void _data_handler(std::string key, void *data, size_t sze)
        {
            if (key != _key)
            {
                // this is not ours!
                return;
            }

            matrix::_data_handler<T>(data, sze, _ringbuf);
        }

        bool _connected;
        std::string _key;
        std::string _km_urn;
        std::string _urn;

        std::shared_ptr<TransportClient> _tc;
        tsemfifo<T> _ringbuf;
        DataMemberCB<DataSink> _cb;
    };

/**
 * Constructor for DataSink.
 *
 * @param km_urn: Access to the keymaster.
 *
 */

    template <typename T, typename U>
    DataSink<T, U>::DataSink(std::string km_urn, size_t ringbuf_size)
        : _connected(false),
          _km_urn(km_urn),
          _ringbuf(ringbuf_size),
          _cb(this, &DataSink::_data_handler)

    {
    }

/**
 * Destructor for DataSink. Disconnects from the data source
 *
 */

    template <typename T, typename U>
    DataSink<T, U>::~DataSink() throw()
    {
        try
        {
            disconnect();
        }
        catch (KeymasterException e)
        {
            std::cerr << e.what() << std::endl;
        }
        catch (zmq::error_t e)
        {
            std::cerr << e.what() << std::endl;
        }
        catch (std::exception e)
        {
            std::cerr << e.what() << std::endl;
        }
    }

/**
 * Helper that checks to see if the DataSink is connected to a
 * source. If not it throws a MatrixException.
 *
 */

    template <typename T, typename U>
    void DataSink<T, U>::_check_connected()
    {
        if (!_connected)
        {
            throw MatrixException("DataSink", "DataSink is not connected.");
        }
    }

/**
 * Performs a blocking get for the data source's data. Will block
 * indefinitely waiting for it.
 *
 * @param val: The data from the data source
 *
 */

    template <typename T, typename U>
    void DataSink<T, U>::get(T &val)
    {
        _check_connected();
        _ringbuf.get(val);
    }

/**
 * Performs a non-blocking get for the data source's data. Will not
 * block if there is no data, instead returning 'false'
 * immediately. If there is data it returns 'true', adn the data is valid.
 *
 * @param val: The data from the data source.
 *
 * @return 'true' if there is data waiting. The data in 'val' will be
 * valid. 'false' if there is no data in the queue, and 'val' is left
 * as it was received.
 *
 */

    template <typename T, typename U>
    bool DataSink<T, U>::try_get(T &val)
    {
        _check_connected();
        return _ringbuf.try_get(val);
    }

/**
 * Performs a blocking get for the data source's data, with
 * time-out. Will block for the specified time while waiting for it.
 *
 * @param val: The data from the data source
 *
 * @param time_out: the time-out, in nanoseconds (relative)
 *
 */

    template <typename T, typename U>
    bool DataSink<T, U>::timed_get(T &val, Time::Time_t time_out)
    {
        _check_connected();
        return _ringbuf.timed_get(val, time_out);
    }

/**
 * Connects to a data source. DataSink does this by obtaining a
 * pointer to a TransportClient and subscribing to the desired key,
 * which it creates here from the <component_name>.<data_name>.
 *
 * A data source may implement several transports, and the client must
 * choose one of them in that case. `connect()` allows the caller to
 * pass in a strategy pattern functor that does this. If no strategy
 * is provided, the 'select_only' pattern will be used. This pattern
 * looks for one and only one transport for the source, and if more
 * than one are available throws a TransportClient::CreationError
 * exception is thrown.
 *
 * @param component_name: The name of the component where the data
 * source originates.
 *
 * @param dta_name: The name of the data on the component.
 *
 * @param tss: transport selection strategy. The default is
 * 'select_only', which returns a transport only if there is only one
 * transport provided (i.e. no choice needs to be made). Also provided
 * is 'select_specified', which will return the specified transport or
 * throw if it does not exist.
 *
 */

    template <typename T, typename U>
    void DataSink<T, U>::connect(std::string component_name,
                                            std::string data_name, std::string transport)
    {
        U tss(_km_urn, transport);

        if (_connected)
        {
            disconnect();
        }

        _urn = tss(component_name, data_name);;
        _key = component_name + "." + data_name;
        _tc = TransportClient::get_transport(_urn);
        _tc->connect(_urn);
        _tc->subscribe(_key, &_cb);
        _connected = true;
    }

/**
 * Disconnects from the data source. This essentially means that the
 * DataSink unsubscribes from the TransportClient, which may or may
 * not continue living. It also resets its shared_ptr to the
 * TransportClient, and calls the static
 * TransportClient::release_transport(). If the only remaining
 * shared_ptr to that TransportClient is in the static
 * TransportClient::transports map, then the TransportClient
 * disconnects from the TransportServer and is destroyed.
 *
 */

    template <typename T, typename U>
    void DataSink<T, U>::disconnect()
    {
        if (_connected)
        {
            _tc->unsubscribe(_key);
            _key.clear();
            _tc.reset();
            TransportClient::release_transport(_urn);
            _connected = false;
        }
    }

/**
 * Returns the number of items waiting in the receive ringbuffer.
 *
 * @return A size_t indicating the number of items ready to be read.
 *
 */

    template <typename T, typename U>
    size_t DataSink<T, U>::items()
    {
        return (size_t)_ringbuf.size();
    }
}


#endif
