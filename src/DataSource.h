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
#include "DataInterface.h"

#include <vector>

namespace matrix
{

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
        ~DataSource() throw();

        bool publish(T &);

    private:
        std::string _km_urn;
        std::string _component_name;
        std::string _transport_name;
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
        _transport_name = km.get_as<std::string>("components."
                                                 + component_name
                                                 + ".Sources."
                                                 + data_name);
        _ts = TransportServer::get_transport(km_urn, _component_name, _transport_name);
    }

    template <typename T>
    DataSource<T>::~DataSource() throw()
    {
        _ts.reset();
        TransportServer::release_transport(_component_name, _transport_name);
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
    inline bool DataSource<std::string>::publish(std::string &val)
    {
        return _ts->publish(_key, val);
    }

/**
 * Specialization for matrix::GenericBuffer version.
 *
 * @param val: A matrix::GenericBuffer acting as a buffer, whose
 * internal buffer contains the bytes to be published.
 *
 * @return true if the put succeeds, false otherwise.
 *
 */

    template <>
    inline bool DataSource<matrix::GenericBuffer>::publish(matrix::GenericBuffer &val)
    {
        return _ts->publish(_key, val.data(), val.size());
    }

}

#endif
