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

#include "matrix/yaml_util.h"
#include "matrix/matrix_util.h"
#include "matrix/Thread.h"
#include "matrix/TCondition.h"
#include "matrix/tsemfifo.h"

#include <string>
#include <vector>
#include <exception>
#include <stdexcept>
#include <sstream>
#include <tuple>

#include <boost/shared_ptr.hpp>
#include <yaml-cpp/yaml.h>
#include <zmq.hpp>

#ifdef __GNUC__
#define __classmethod__  __PRETTY_FUNCTION__
#else
#define __classmethod__  __func__
#endif

namespace matrix
{
    class KeymasterServer
    {
    public:

        KeymasterServer(std::string configfile);

        KeymasterServer(YAML::Node n);

        ~KeymasterServer();

        void run();

        void terminate();

    private:

        struct KmImpl;

        boost::shared_ptr<KeymasterServer::KmImpl> _impl;
    };

    class KeymasterException : public matrix::MatrixException
    {
    public:
        KeymasterException(std::string msg) :
                matrix::MatrixException("KeymasterException", msg)
        {
        }
    };

/**
 * \class KeymasterCallbackBase
 *
 * A virtual pure base callback class. A pointer to one of these is
 * given when subscribing for a keymaster value. When the value is
 * published, it is received by the Keymaster client object, which then
 * calls the provided pointer to an object of this type.
 *
 */

    struct KeymasterCallbackBase
    {
        void operator()(std::string key, YAML::Node val)
        {
            _call(key, val);
        }

        void exec(std::string key, YAML::Node val)
        {
            _call(key, val);
        }

    private:
        virtual void _call(std::string key, YAML::Node val) = 0;
    };

/**
 * \class KeymasterHeartbeatCB
 *
 * This sublcass of the base KeymasterCallbackBase is specifically
 * meant to resond do Keymaster hearbeat publications. The Keymaster
 * updates its heartbeat, which is a Time::Time_t containing the
 * current time of the update, every second. Thus a client may have
 * one of these handling a heartbeat subscription and thus easily see
 * whether the KeymasterServer is still running, merely by reading the
 * current time and comparing it to the heartbeat time.
 *
 */

    struct KeymasterHeartbeatCB : public matrix::KeymasterCallbackBase
    {
        Time::Time_t last_update()
        {
            Time::Time_t t;
            matrix::ThreadLock<matrix::Mutex> l(lock);
            l.lock();
            t = last_heard;
            l.unlock();
            return t;
        }

    private:
        void _call(std::string key, YAML::Node val)
        {
            matrix::ThreadLock<matrix::Mutex> l(lock);
            l.lock();
            last_heard = val.as<Time::Time_t>();
            l.unlock();
        }

        matrix::Mutex lock;
        Time::Time_t last_heard;
    };

/**
 * \class KeymasterMemberCB
 *
 * A subclassing of the base KeymasterCallbackBase callback class that allows
 * a using class to use one of its methods as the callback.
 *
 * example:
 *
 *     class foo()
 *     {
 *     public:
 *         void bar(YAML::Node n) {...}
 *
 *     private:
 *         KeymasterMemberCB<foo> my_cb;
 *         Keymaster *km;
 *     };
 *
 *     foo::foo()
 *       :
 *       my_cb(this, &foo::bar)
 *     {
 *         ...
 *         km = new Keymaster(km_url);
 *         km->subscribe("foo.bar.baz", &my_cb);
 *     }
 *
 */

    template<typename T>
    class KeymasterMemberCB : public matrix::KeymasterCallbackBase
    {
    public:
        typedef void (T::*ActionMethod)(std::string, YAML::Node);

        KeymasterMemberCB(T *obj, ActionMethod cb) :
                _object(obj),
                _faction(cb)
        {
        }

    private:
        ///
        /// Invoke a call to the user provided callback
        ///
        void _call(std::string key, YAML::Node val)
        {
            if (_object && _faction)
            {
                (_object->*_faction)(key, val);
            }
        }

        T *_object;
        ActionMethod _faction;
    };


    class Keymaster
    {
    public:

        Keymaster(std::string keymaster_url, bool shared = false);

        ~Keymaster();

        YAML::Node get(std::string key);

        bool get(std::string key, ::mxutils::yaml_result &yr);

        bool put(std::string key, YAML::Node n, bool create = false);

        void put_nb(std::string key, std::string val, bool create = true);

        bool del(std::string key);

        bool subscribe(std::string key, matrix::KeymasterCallbackBase *f);

        bool unsubscribe(std::string key);

        template<typename T>
        T get_as(std::string key);

        template<typename T>
        bool put(std::string key, T v, bool create = false);

        ::mxutils::yaml_result get_last_result();

    private:

        void _subscriber_task();

        void _put_task();

        void _run();

        void _run_put();

        void _handle_keymaster_server_exception();

        ::mxutils::yaml_result _call_keymaster(std::string cmd, std::string key,
                                             std::string val = "", std::string flag = "");

        std::shared_ptr<zmq::socket_t> _keymaster_socket();

        std::shared_ptr<zmq::socket_t> _km_;
        ::mxutils::yaml_result _r;
        std::string _km_url;
        std::string _pipe_url;
        std::vector<std::string> _km_pub_urls;

        std::map<std::string, matrix::KeymasterCallbackBase *> _callbacks;
        matrix::Thread<Keymaster> _subscriber_thread;
        matrix::TCondition<bool> _subscriber_thread_ready;
        matrix::Thread<Keymaster> _put_thread;
        matrix::TCondition<bool> _put_thread_ready;
        bool _put_thread_run;
        matrix::tsemfifo<std::tuple<std::string, std::string, bool> > _put_fifo;
        matrix::Mutex _shared_lock;
    };

    template<typename T>
    T Keymaster::get_as(std::string key)
    {
        return get(key).as<T>();
    }

    template<typename T>
    bool Keymaster::put(std::string key, T v, bool create)
    {
        YAML::Node n(v);
        return put(key, n, create);
    }
}; // namespace matrix

#endif
