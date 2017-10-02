/*******************************************************************
 *  GnuradioDataSource.h - A template class that provides an interface to
 *  publish data into a gr-zeromq SUB-SOURCE block.
 *
 *  Copyright (C) 2017 Associated Universities, Inc. Washington DC, USA.
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
 *  Green Bank Observatory
 *  P. O. Box 2
 *  Green Bank, WV 24944-0002 USA
 *
 *******************************************************************/

#if !defined(GnuradioDataSource_h)
#define GnuradioDataSource_h

#include "matrix/Keymaster.h"

#include <vector>
#include <complex>
#include "matrix/zmq_util.h"
#include "matrix/ZMQContext.h"

namespace matrix
{


/**
 * \class DataSource
 *
 * A quick experimental method of publishing data to gnuradio-zeromq blocks.
 * NOTE: A currently only the float and std::complex<float> types are supported.
 * (A limitation of Gnu Radio.)
 *
 * The ZMQ URL is specified in the config file under the key e.g:
 *     components.component_name.grc_url.data_name: tcp://*:5555
 *
 * As an example the YAML in the config file for a component named siggen publishing
 * output_data would be:
 *
 * components:
 *     siggen:
 *         type: SignalGenerator
 *         grc_url:
 *             output_data: tcp://*:5555
 *
 * In a component, the usage would look something like:
 * class SignalGenerator : public Component
 * {
 * ...
 * GnuradioDataSource<float> grc_source;
 * ...
 * };
 *
 * SignalGenerator::SignalGenerator(name, km_url) :
 * grc_source(km_url, my_instance_name, "output_data")
 * ...
 *
 * Like a normal matrix DataSource, once constructed can be used in any state,
 * but shouldn't publish in the Standby state.
 *
 */

    template <typename T>
    class GnuradioDataSource
    {
    public:
        GnuradioDataSource(std::string km_urn, std::string component_name, std::string data_name);
        ~GnuradioDataSource() throw();

        bool publish(T &);
        bool publish(T *val, size_t nelements);
        bool connect();
        void disconnect();

    private:
        std::string _km_urn;
        std::string _component_name;
        std::string _zmq_address;
        std::string _data_name;
        std::unique_ptr<zmq::socket_t> _sock;
    };

/**
 * Creates a GnuradioDataSource, given a data name, a keymaster urn, and a
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
    GnuradioDataSource<T>::GnuradioDataSource(std::string km_urn,
                                              std::string component_name,
                                              std::string data_name)
        :
          _km_urn(km_urn),
          _component_name(component_name),
          _zmq_address(""),
          _data_name(data_name),
          _sock()
    {
        matrix::Keymaster km(km_urn);
        // obtain the transport name associated with this data source and
        // get a pointer to that transport
        _zmq_address = km.get_as<std::string>("components."
                                                 + component_name
                                                 + ".grc_url."
                                                 + data_name);
        _sock.reset( new zmq::socket_t(matrix::ZMQContext::Instance()->get_context(), ZMQ_PUB) );

        connect();

    }

    template <typename T>
    GnuradioDataSource<T>::~GnuradioDataSource() throw()
    {
        disconnect();
    }

    template<typename T>
    void GnuradioDataSource<T>::disconnect()
    {
        _sock->unbind(_zmq_address.c_str());
    }

/**
 * Puts a value of type 'T' to the data source. 'T' must be a
 * contiguous type: a POD, or struct of PODS, or an array of such.
 *
 * NOTE: Currently grc only understands 2 datatypes:
 *     float
 *     complex [float real, float imag]
 *
 * @param data: The data to send.
 *
 * @return true if the put succeeds, false otherwise.
 *
 */

    template <typename T>
    bool GnuradioDataSource<T>::publish(T &val)
    {
        return _sock->send((void *)&val, sizeof(val), 0);
    }

    template <typename T>
    bool GnuradioDataSource<T>::publish(T *val, size_t nelements)
    {
        return _sock->send((void *)val, nelements * sizeof(T), 0);
    }

    template <typename T>
    bool GnuradioDataSource<T>::connect()
    {
        try
        {
            _sock->bind(_zmq_address.c_str());
            std::cout << __PRETTY_FUNCTION__ << " bind to " << _zmq_address << " ok!" << std::endl;
            return true;
        }
        catch (...)
        {
            std::cout << __PRETTY_FUNCTION__ << " bind to " << _zmq_address << " failed" << std::endl;
            return false;
        }
    }
}

#endif
