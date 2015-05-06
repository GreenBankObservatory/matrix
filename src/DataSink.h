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
#include <string>

namespace matrix
{
    template <typename T>
    class DataSink
    {
    public:
        DataSink(std::string km_urn, size_t ringbuf_size = 10);
        ~DataSink() throw() {};

        void get(T &);
        bool try_get(T &);

        bool connect(std::string component_name, std::string data_name,
                     std::shared_ptr<transport_selection_strategy> tss = {});
        bool disconnect();

    private:
        void _check_connected();
        void _data_handler(std::string, void *, size_t);

        bool _connected;
        std::string _key;
        std::string _km_urn;

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

    template <typename T>
    DataSink<T>::DataSink(std::string km_urn, size_t ringbuf_size)
        : _connected(false),
          _km_urn(km_urn),
          _ringbuf(ringbuf_size),
          _cb(this, &DataSink::_data_handler)

    {
    }

/**
 * Destructor for DataSink. Disconnects from the data source, and
 * cleans up its Keymaster client.
 *
 */

    template <typename T>
    DataSink<T>::~DataSink()
    {
        try
        {
            disconnect();
        }
        catch (KeymasterException e)
        {
            cerr << e.what() << endl;
        }
        catch (zmq::error_t e)
        {
            cerr << e.what() << endl;
        }
        catch(exception e)
        {
            cerr << e.what() << endl;
        }
    }

/**
 * Helper that checks to see if the DataSink is connected to a
 * source. If not it throws a MatrixException.
 *
 */

    template <typename T>
    void DataSink<T>::_check_connected()
    {
        if (!_connected)
        {
            throw MatrixException("DataSink is not connected.");
        }
    }

/**
 * This provides a means for the TransportClient to pass off data to
 * the DataSink, in a form the DataSink understands. In this version,
 * the type of the data being given is known, and though it is handed off
 * as a void * by the TransportClient the DataSink assumes it is of
 * typename T and does the appropriate cast as it posts it into the ringbuffer.
 *
 * @param key: The data key. This should match the key used to
 * subscribe. If it doesnt the function returns (TBF?)
 *
 * @param buf: The actual data, assumed to be a chunk of memory
 * occupied by a type T.
 *
 * @param sze: The size of 'buf', it had better match the sizeof(T).
 *
 */

    template <typename T>
    void DataSink<T>::_data_handler(std::string key, void *buf, size_t sze)
    {
        if (key != _key)
        {
            // this is not ours!
            return;
        }

        if (sizeof(T) != sze)
        {
            // this would be a pretty bad error. It is ours, but the
            // sizes are wrong!
            throw MatrixException("DataSink::_data_handler(): size mismatch error.");
        }

        // TBF: What to do if ringbuf is full? Throw? Ignore?
        _ringbuf.try_put((T)(*buf));
    }

/**
 * This is a specialization of _data_handler() for std::string. A
 * DataSink<std::string> doesn't expect the data to be of a single
 * size. In this case the data can be an ASCII string, a variable
 * length binary buffer, etc. (Note that a T can be used to achieve
 * this effect as well: imagine a T that is a protobuf, or a
 * MessagePack, etc.)
 *
 * @param key: The data key. This should match the key used to
 * subscribe. If it doesnt the function returns (TBF?)
 *
 * @param buf: The actual data, assumed to be a chunk of memory
 * occupied by a type T.
 *
 * @param sze: The size of 'buf', it had better match the sizeof(T).
 *
 */

    template <>
    void DataSink<std::string>::_data_handler(std::string key, void *buf, size_t sze)
    {
        if (key != _key)
        {
            // this is not ours!
            return;
        }

        std::string val(sze, 0);
        std::memmove((char *)val.data(), buf, sze);
        _ringbuf.try_put(val);
    }

/**
 * Performs a blocking get for the data source's data. Will block
 * indefinitely waiting for it.
 *
 * @param val: The data from the data source
 *
 */

    template <typename T>
    void DataSink<T>::get(T &val)
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

    template <typename T>
    bool DataSink<T>::try_get(T &val)
    {
        _check_connected();
        return _ringbuf.try_get(val);
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

    template <typename T>
    void DataSink<T>::connect(string component_name, string data_name,
                              shared_ptr<transport_selection_strategy> tss)
    {
        if (_connected)
        {
            disconnect();
        }

        if (!tss)
        {
            tss.reset(new select_only(_km_urn));
        }

        _key = component_name + "." + data_name;
        _urn = tss(component_name, data_name);
        _tc = TransportClient::get_transport(_urn);
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

    template <typename T>
    void DataSink<T>::disconnect()
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

}

#endif
