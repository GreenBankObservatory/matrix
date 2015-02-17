/*******************************************************************
 *  Keymaster.h - A YAML-based key/value store, accessible via 0MQ
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

#if !defined(_KEYMASTER_H_)
#define _KEYMASTER_H_

#include "yaml_util.h"

#include <string>
#include <vector>
#include <exception>
#include <stdexcept>
#include <sstream>

#include <boost/shared_ptr.hpp>
#include <yaml-cpp/yaml.h>
#include <zmq.hpp>

class KeymasterServer
{
public:

    KeymasterServer(std::string configfile);
    ~KeymasterServer();

    void run();
    void terminate();

private:

    struct KmImpl;

    boost::shared_ptr<KeymasterServer::KmImpl> _impl;
};

class KeymasterException : public std::runtime_error
{
public:

    KeymasterException(std::string msg)
        : runtime_error("Keymaster exception"),
          _msg(msg)
    {
    }

    virtual ~KeymasterException() throw ()
    {
    }


    virtual const char* what() const throw()
    {
        std::ostringstream msg;
        msg << std::runtime_error::what() << ": " << _msg;
        return msg.str().c_str();
    }

private:

    std::string _msg;
};

class Keymaster
{
public:

    Keymaster(std::string keymaster_url);
    ~Keymaster();

    YAML::Node get(std::string key);
    template <typename T>
    T get_as(std::string key);

    void put(std::string key, YAML::Node n, bool create = false);
    template <typename T>
    void put(std::string key, T v, bool create = false);

    void del(std::string key);

    void subscribe(std::string key /*, functor */);
    void unsubscribe(std::string key);

    mxutils::yaml_result get_last_result() {return _r;}

private:

    zmq::context_t &_ctx;
    zmq::socket_t _km;
    mxutils::yaml_result _r;

};

template <typename T>
T Keymaster::get_as(std::string key)
{
    return get(key).as<T>();
}

template <typename T>
void Keymaster::put(std::string key, T v, bool create)
{
    YAML::Node n(v);
    put(key, n, create);
}

#endif
