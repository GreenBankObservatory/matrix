/*******************************************************************
 *  Keymaster.cc - Implements a YAML-based 0MQ key/value store
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

#include "Keymaster.h"
#include "ZMQContext.h"
#include "ThreadLock.h"
#include "tsemfifo.h"
#include "Thread.h"
#include "ThreadLock.h"
#include "zmq_util.h"
#include "netUtils.h"
#include "yaml_util.h"

#include <string>
#include <cstring>
#include <sstream>
#include <map>
#include <vector>
#include <iostream>
#include <sstream>
#include <exception>
#include <algorithm>

#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#include <boost/circular_buffer.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/shared_ptr.hpp>

using namespace std;
using namespace mxutils;

#define SUBSCRIBE   1
#define UNSUBSCRIBE 2
#define QUIT        3
#define KM_TIMEOUT  5000

struct substring_p
{
    substring_p(string subs)
        : _subs(subs)
    {
    }

    bool operator()(const string s)
    {
        return !(s.find(_subs) == string::npos);
    }

    string _subs;
};

struct same_transport_p
{
    same_transport_p(string url)
    {
        vector<string> parts;
        boost::split(parts, url, boost::is_any_of(":"));

        if (!parts.empty())
        {
            _transport = parts[0];
        }
    }

    bool operator()(const string s)
    {
        substring_p pred(_transport);
        return pred(s);
    }

    string _transport;
};

/**
 * KmImpl is the private implementation of the KeymasterServer class.
 *
 */

struct KeymasterServer::KmImpl
{
    KmImpl(YAML::Node config);
    ~KmImpl();

    struct data_package
    {
        std::string key;
        YAML::Node node;
    };

    void server_task();
    void state_manager_task();
    bool load_config_file(string filename);
    bool publish(std::string key);
    void run();
    void terminate();

    void setup_urls();
    bool using_tcp();
    void bind_server(zmq::socket_t &server_sock, vector<string> &urls, bool transient);

    Thread<KmImpl> _server_thread;
    Thread<KmImpl> _state_manager_thread;
    TCondition<bool> _server_thread_ready;
    TCondition<bool> _state_manager_thread_ready;

    int _state_port_used;
    int _pub_port_used;
    bool _state_manager_done;
    tsemfifo<data_package> _data_queue;
    Mutex _cache_lock;
    std::string _state_task_url;
    std::string _hostname;
    bool _state_task_quit;

    // The service URLs. Each interface (STATE or PUBLISH) may have
    // multiple URLs (tcp, inproc, ipc) for possible future
    // use. Subscribers will need the publisher service urls.
    std::vector<std::string> _state_service_urls;
    std::vector<std::string> _publish_service_urls;

    YAML::Node _root_node;    //<? THE keymaster node
};

/**
 * Constructor of the implementation class of the KeymasterServer.  The
 * implementation class handles all the details; the KeymasterServer class
 * merely provides the external interface, and loads the configuration file.
 *
 * @param config: The YAML configuration file. Specifies the Keymaster
 * server configuration and also the initial state of the Keymaster's
 * data store.
 *
 * KeymasterServer supports a 0MQ REQ/REP service which allows the
 * Keymaster client to query for values, alter values, create values and
 * delete values. KeymasterServer also publishes values that have been
 * changed by any client to all interested subscribers. These URLs are
 * not well known however. The Keymaster client class obtains these via
 * request, so that it can then subscribe.
 *
 */

KeymasterServer::KmImpl::KmImpl(YAML::Node config)
:
    _server_thread(this, &KeymasterServer::KmImpl::server_task),
    _state_manager_thread(this, &KeymasterServer::KmImpl::state_manager_task),
    _server_thread_ready(false),
    _state_manager_thread_ready(false),
    _data_queue(1000),
    _state_task_url(string("inproc://") + gen_random_string(20)),
    _state_task_quit(true)
{
    int i = 0;

    _root_node = YAML::Clone(config);
    setup_urls();

    if (using_tcp() && !getCanonicalHostname(_hostname))
    {
        ostringstream msg;
        msg << "KeymasterServer: TCP transport requested, but unable to obtain canonical hostname:"
            << strerror(errno) << ends;
        throw(runtime_error(msg.str()));
    }
}

/**
 * Destructor. Signals the server thread that we're done, waiting for it to exit
 * on its own.
 *
 */

KeymasterServer::KmImpl::~KmImpl()

{
    vector<string>::const_iterator i;

    terminate();

    i = find_if(_publish_service_urls.begin(), _publish_service_urls.end(), substring_p("ipc"));

    if (i != _publish_service_urls.end())
    {
        unlink(i->c_str());
    }
}

/**
 * Starts the keymaster threads.
 *
 */

void KeymasterServer::KmImpl::run()
{
    if (!_server_thread.running())
    {
        if (_server_thread.start() != 0)
        {
            throw(runtime_error("KeymasterServer: unable to start publishing thread"));
        }
    }

    if (_server_thread_ready.wait(true, 1000000) != true)
    {
        throw(runtime_error("KeymasterServer: timed out waiting for publishing thread"));
    }

    // Make sure this is run AFTER the _server_thread (publisher)
    // because it will put publishing information in the _root_node. All
    // access to the _root_node should be via the _state_manager_thread
    // because the _root_node is not thread-safe.
    if (!_state_manager_thread.running())
    {
        if (_state_manager_thread.start() != 0 || !_state_manager_thread_ready.wait(true, 1000000))
        {
            throw(runtime_error("KeymasterServer: unable to start request thread"));
        }
    }
}

/**
 * Terminates the Keymaster server threads cleanly.
 *
 */

void KeymasterServer::KmImpl::terminate()
{
    if (_state_manager_thread.running())
    {
        zmq::socket_t sock(ZMQContext::Instance()->get_context(), ZMQ_PAIR);
        sock.connect(_state_task_url.c_str());
        z_send(sock, _state_task_quit, 0);
        sock.close();
        _state_manager_thread.stop_without_cancel();
    }

    if (_server_thread.running())
    {
        _data_queue.release();
        _server_thread.stop_without_cancel();
    }
}

/**
 * Sets up and validates all the urls. The URLs are retrieved from the
 * root node.
 *
 * @return bool, false if there is a problem, true otherwise.
 *
 */

void KeymasterServer::KmImpl::setup_urls()
{
    vector<string>::const_iterator cvi;
    vector<string> urls = _root_node["Keymaster"]["URLS"]["Initial"].as<vector<string> >();

    for (cvi = urls.begin(); cvi != urls.end(); ++cvi)
    {
        string lc(cvi->size(), 0);
        transform(cvi->begin(), cvi->end(), lc.begin(), ::tolower);
        _state_service_urls.push_back(lc);

        if (lc.find("tcp") != string::npos)
        {
            // for now just copy the state service URL. Later, port will
            // be replaced by an ephemeral port for this.
            _publish_service_urls.push_back(lc);
        }
        else if (lc.find("ipc") != string::npos)
        {
            string s = lc + ".publisher.";
            _publish_service_urls.push_back(s);
        }
        else if (lc.find("inproc") != string::npos)
        {
            string s = lc + ".publisher.";
            _publish_service_urls.push_back(s);
        }
        else
        {
            ostringstream msg;
            msg << "KeymasterServer: Unrecognized URL: " << *cvi << ends;
            throw(runtime_error(msg.str()));
        }
    }
}

/**
 * Checks to see if TCP transport is required, by examining the state
 * service URLs (publisher service URLs will mirror these)
 *
 * @return bool, true if TCP is required, false otherwise.
 *
 */

bool KeymasterServer::KmImpl::using_tcp()
{
    bool rval = false;
    vector<string>::const_iterator cvi;

    if (find_if(_state_service_urls.begin(), _state_service_urls.end(), substring_p("tcp"))
        != _state_service_urls.end())
    {
        rval = true;
    }

    return rval;
}

/**
 * Binds a given server socket to the provided urls, optionally using
 * transient addresses.
 *
 * @param server_sock: The server. May be a ZMQ_PUB or ZMQ_REP.
 *
 * @param urls: A list of URLs to bind to. These are presumed to be
 * valid. IPC & inproc urls given with the `transient` flag will have a
 * random string appended to them to avoid collisions. These URLs will
 * then all be replaced by the one actually used, except the TCP one (if
 * included) being completely replaced in all cases by the string
 * "tcp://<hostname>:<port>, whether the port is transient or not.
 *
 * @param transient: A flag, if true the TCP and IPC urls (if provided)
 * will be bound to a transient port (TCP) or to a temporary pipe
 * (IPC). If false, the URLs are used as is.
 *
 */

void KeymasterServer::KmImpl::bind_server(zmq::socket_t &server_sock, vector<string> &urls, bool transient)
{
    vector<string>::iterator cvi;

    // bind to all publisher URLs
    for (cvi = urls.begin(); cvi != urls.end(); ++cvi)
    {
        if (cvi->find("tcp") != string::npos)
        {
            // it's TCP. Use the URL, but replace the port with an
            // ephemeral port.
            if (transient)
            {
                int port_used = zmq_ephemeral_bind(server_sock, "tcp://*:*", 1000);
                ostringstream tcp_url;
                tcp_url << "tcp://" << _hostname << ":" << port_used;
                *cvi = tcp_url.str();
            }
            else
            {
                vector<string> fields;
                boost::split(fields, *cvi, boost::is_any_of(":"));
                server_sock.bind(cvi->c_str());
                ostringstream tcp_url;
                tcp_url << "tcp://" << _hostname << ":" << fields.back();
                *cvi = tcp_url.str();
            }
        }
        else
        {
            // A temporary name for IPC or inproc is needed for the same
            // reason the transient port is needed for TCP: to avoid
            // collisions if more than one keymaster is running on the
            // same machine.  I (RC) am aware of the man page's
            // exhortation to "never use mktemp()".  mktemp() does
            // exactly what is needed here: generate a temporary name
            // and let 0MQ use it. mkstemp() *opens* the file as well,
            // which is not wanted. No security issues are anticipated
            // here. (possible alternative: use mkdtemp())
            if (transient)
            {
                *cvi += gen_random_string(6);
            }

            server_sock.bind(cvi->c_str());
        }
    }
}

/**
 * This is the publisher server task.  It sits on the queue waiting
 * for something to be published until it gets a "QUIT" message.  This
 * consists of releasing the state queue.
 *
 */

void KeymasterServer::KmImpl::server_task()

{
    data_package dp;
    zmq::socket_t data_publisher(ZMQContext::Instance()->get_context(), ZMQ_PUB);
    string tcp_url;

    try
    {
        bind_server(data_publisher, _publish_service_urls, true);
    }
    catch (zmq::error_t e)
    {
        cerr << "Error in KeymasterServer publisher thread: " << e.what() << endl
             << "Exiting KeymasterServer publishing thread." << endl;
        return;
    }

    _server_thread_ready.signal(true); // Allow constructor to move on

    while (_data_queue.get(dp))
    {
        // Publish data. Whenever a node is modified, we need to of
        // course publish that node. But we also need to publish
        // upstream nodes, because someone may have subscribed to them,
        // and a change to this key means a change to all upstream
        // keys. So if the node is "foo.bar.baz", we publish "foo",
        // "foo.bar", and "foo.bar.baz"

        try
        {
            // Publish "Root" if there is no key
            if (dp.key.empty())
            {
                ostringstream yr;
                yr << dp.node;
                z_send(data_publisher, string("Root"), ZMQ_SNDMORE);
                z_send(data_publisher, yr.str(), 0);
            }
            else
            {
                vector<string> keys;
                boost::split(keys, dp.key, boost::is_any_of("."));

                // Publish with keys
                for (int i = 1; i < keys.size() + 1; ++i)
                {
                    string key = boost::algorithm::join(vector<string>(keys.begin(), keys.begin() + i), ".");
                    yaml_result r = get_yaml_node(dp.node, key);

                    if (r.result == true)
                    {
                        ostringstream yr;
                        // we just need the node that goes with the key.
                        yr << r.node;
                        z_send(data_publisher, key, ZMQ_SNDMORE);
                        z_send(data_publisher, yr.str(), 0);
                    }
                }
            }
        }
        catch (zmq::error_t e)
        {
            cerr << "ZMQ exception in publisher thread: "
                 << e.what() << endl;
        }
    }

    // Done. Clean up.
    int zero = 0;
    data_publisher.setsockopt(ZMQ_LINGER, &zero, sizeof zero);
    data_publisher.close();
}

/**
 * This 0MQ server task implements a REPLY socket (of a REQ/REP pair),
 * whose function is to receive requests for cached data.  This is
 * useful for late joiners, especially for keys that don't change
 * frequently.
 *
 */

void KeymasterServer::KmImpl::state_manager_task()

{
    zmq::context_t &ctx = ZMQContext::Instance()->get_context();
    zmq::socket_t state_sock(ctx, ZMQ_REP);
    zmq::socket_t pipe(ctx, ZMQ_PAIR);  // mostly to tell this task to go away

    try
    {
        // control pipe
        pipe.bind(_state_task_url.c_str());
    }
    catch (zmq::error_t e)
    {
        cerr << "Error in state manager thread: " << e.what() << endl
             << "Exiting state thread." << endl
             << "_state_task_url = " << _state_task_url << endl;
        return;
    }

    try
    {
        // bind to all state server URLs
        bind_server(state_sock, _state_service_urls, false);
        put_yaml_val(_root_node, "KeymasterServer.URLS", _state_service_urls, true);
    }
    catch (zmq::error_t e)
    {
        cerr << "Error in state manager thread: " << e.what() << endl
             << "Exiting state thread." << endl
             << "_state_service_urls = " << endl;
        output_vector(_state_service_urls, cerr);
        cerr << endl;
        
        return;
    }


    yaml_result rs = put_yaml_val(_root_node, "Keymaster.URLS.AsConfigured.State", _state_service_urls, true);
    yaml_result rp = put_yaml_val(_root_node, "Keymaster.URLS.AsConfigured.Pub", _publish_service_urls, true);

    if (! (rs.result && rp.result))
    {
        cerr << "Error storing configured URLs into the root node." << endl
             << "Exiting state thread." << endl;
        return;
    }

    zmq::pollitem_t items [] =
        {
            { pipe, 0, ZMQ_POLLIN, 0 },
            { state_sock, 0, ZMQ_POLLIN, 0 }
        };

    _state_manager_thread_ready.signal(true); // allow 'run()' to move
                                              // on.
    while (1)
    {
        try
        {
            zmq::poll(&items [0], 2, -1);

            if (items[0].revents & ZMQ_POLLIN)
            {
                bool quit;
                z_recv(pipe, quit);

                if (_state_task_quit == quit)
                {
                    break;
                }
            }

            if (items[1].revents & ZMQ_POLLIN)
            {
		string key;
                vector<string> frame;

		z_recv(state_sock, key);

		// Determine the request.  Currently requests may
		// be either a "ping" (just to see if the service
		// is alive); a "LIST" to get information on all
		// samplers and parameters published.  If none of
		// the above, the request is assumed to be a key
		// to a published item.

		if (key.size() == 4 && key == "ping")
		{
                    // read any remaining parts
                    z_recv_multipart(state_sock, frame);

		    // reply with something
		    z_send(state_sock, "I'm not dead yet!", 0);
		}
                /////////////////// G E T ///////////////////
		else if (key.size() == 3 && key == "GET")
		{
                    z_recv_multipart(state_sock, frame);

                    if (!frame.empty())
                    {
                        string keychain = frame[0];
                        ostringstream rval;

                        if (keychain == "Root")
                        {
                            keychain = "";
                        }

                        yaml_result r = get_yaml_node(_root_node, keychain);
                        rval << r;
                        z_send(state_sock, rval.str(), 0);
                    }
                    else
                    {
                        string msg("ERROR: Keychain expected, but not received!");
                        z_send(state_sock, msg, 0);
                    }
		}
                /////////////////// P U T ///////////////////
                else if (key.size() == 3 && key == "PUT")
                {
                    z_recv_multipart(state_sock, frame);

                    if (frame.size() > 1)
                    {
                        string keychain = frame[0];

                        if (keychain == "Root")
                        {
                            keychain = "";
                        }

                        string yaml_string = frame[1];
                        bool create = false;

                        if (frame.size() > 2 && frame[2] == "create")
                        {
                            create = true;
                        }

                        yaml_result r;
                        ostringstream rval;
                        YAML::Node n = YAML::Load(yaml_string);
                        r = put_yaml_node(_root_node, keychain, n, create);
                        rval << r;
                        z_send(state_sock, rval.str(), 0);

                        if (r.result)
                        {
                            publish(keychain);
                        }
                    }
                    else
                    {
                        string msg("ERROR: Keychain and value expected, but not received!");
                        z_send(state_sock, msg, 0);
                    }
                }
                /////////////////// D E L ///////////////////
                else if (key.size() == 3 && key == "DEL")
                {
                    z_recv_multipart(state_sock, frame);

                    if (!frame.empty())
                    {
                        string keychain = frame[0];
                        yaml_result r = delete_yaml_node(_root_node, keychain);
                        ostringstream rval;
                        rval << r;
                        z_send(state_sock, rval.str(), 0);

                        if (r.result)
                        {
                            publish(keychain);
                        }
                    }
                    else
                    {
                        string msg("ERROR: Keychain expected, but not received!");
                        z_send(state_sock, msg, 0);
                    }
                }
                else
                {
                    z_recv_multipart(state_sock, frame);
                    ostringstream msg;
                    msg << "Unknown request '" << key;
                    z_send(state_sock, msg.str(), 0);
                }
	    }
        }
        catch (zmq::error_t e)
        {
            cerr << "State manager task, main loop: " << e.what() << endl;
        }
    }

    int zero = 0;
    state_sock.setsockopt(ZMQ_LINGER, &zero, sizeof zero);
    state_sock.close();
}

/**
 * This is the KeymasterServer's Data publishing facility.  It is a private
 * function that does all the work preparing data for publication.
 * The data and metadata are copied into a 'data_package' object and
 * posted on the publication semaphore queue.  Public inline functions
 * such as 'publish_data()' call this with the appropriate parameters
 * set.  This 'publish' function assumes the data is not Sampler or
 * Parameter data.
 *
 * @param key: the data key
 *
 * @return true if the data was succesfuly placed in the publication
 * queue, false otherwise.
 *
 */

bool KeymasterServer::KmImpl::publish(std::string key)
{
    data_package dp;

    dp.key = key;
    // Clone the root YAML::Node. The publisher will be running in a
    // different thread, so do this to avoid locks & ensure only the
    // state thread actually accesses _root_node.
    dp.node = YAML::Clone(_root_node);

    if (!_data_queue.try_put(dp))
    {
        return false;
    }

    return true;
}

/**
 * \class KeymasterServer
 *
 * This is a class that provides a 0MQ publishing facility. KeymasterServer
 * both publishes data as a ZMQ PUB, but also provides a REQ/REP service.
 *
 * The KeymasterServer should be one of the first objects created in the
 * main program entry point:
 *
 *     int main(...)
 *     {
 *         KeymasterServer *kms;
 *         kms = new KeymasterServer("config.yaml");
 *
 *         if (!kms->run())
 *         {
 *             return 1;
 *         }
 *
 *         ...
 *
 *         return 0;
 *     }
 *
 * The Keymaster now is ready to serve clients. The initial state of the
 * KeymasterServer is set by the file 'config.yaml'.
 *
 */

/**
 * KeymasterServer constructor.  Constructs a KeymasterServer from a
 * valid YAML encoded configuration file.
 *
 * @param configfile: A YAML configuration file which sets the
 * Keymaster's data store to an initial state.
 *
 */

KeymasterServer::KeymasterServer(std::string configfile)
{
    YAML::Node config;

    try
    {
        config = YAML::LoadFile(configfile);
    }
    catch (YAML::BadFile e)
    {
        ostringstream msg;
        msg << "KeymasterServer: Could not open config file "
            << configfile;
        throw(runtime_error(msg.str()));
    }

    _impl.reset(new KeymasterServer::KmImpl(config));
}

/**
 * KeymasterServer constructor.  Constructs a KeymasterServer from a
 * valid YAML::Node.
 *
 * @param n: A YAML::Node containing the initial configuration for the
 * Keymaster server.
 *
 */

KeymasterServer::KeymasterServer(YAML::Node n)
{
    _impl.reset(new KeymasterServer::KmImpl(n));
}

/**
 * Destructor, signals the server thread that we're done, waiting for it
 * to exit on its own.
 *
 */

KeymasterServer::~KeymasterServer()

{
    _impl.reset();
}

/**
 * Starts the KeymasterServer running.
 *
 * @return true if the threads were started, false otherwise.
 */

void KeymasterServer::run()
{
    _impl->run();
}

/**
 * Terminates the KeymasterServer threads. This function will block
 * until the threads are done.
 *
 */

void KeymasterServer::terminate()
{
    _impl->terminate();
}

/****************************************************************//**
 * \class Keymaster
 *
 * The Keymaster class is a client to the keymaster service,
 * encapsulating and hiding the details of the connection.
 *
 * Example usage:
 *
 *      Keymaster km("inproc://matrix.keymaster");
 *      string key = "foo.Transports";
 *      vector<string> transports = km.get_as<vector<string> >(key)
 *
 *******************************************************************/

/**
 * The Keymaster client constructor makes a connection to the specified
 * Keymaster service URL. Will throw a KeymasterException if it is
 * unable to make the connection.
 *
 * @param keymaster_url: The url for the keymaster service
 *
 */

Keymaster::Keymaster(string keymaster_url, bool shared)
    :
    _km_url(keymaster_url),
    _pipe_url(string("inproc://") + gen_random_string(20)),
    _subscriber_thread(this, &Keymaster::_subscriber_task),
    _subscriber_thread_ready(false)
{
}

/**
 * The destructor will close the connection to the keymaster service.
 *
 */

Keymaster::~Keymaster()
{
    int zero = 0;

    if (_subscriber_thread.running())
    {
        zmq::socket_t ctrl(ZMQContext::Instance()->get_context(), ZMQ_REQ);
        ctrl.connect(_pipe_url.c_str());
        z_send(ctrl, QUIT, 0);
        int rval;
        z_recv(ctrl, rval);
        _subscriber_thread.stop_without_cancel();
    }

    if (_km_.get())
    {
        ThreadLock<Mutex> lck(_shared_lock);
        _km_->setsockopt(ZMQ_LINGER, &zero, sizeof zero);
        _km_->close();
    }
}

/**
 * RPC call to the Keymaster. Makes this call atomic, so that multiple
 * threads may use one Keymaster client without interrupting the
 * REQ/REPL pairs.
 *
 * @param cmd: One of the commands recognized by the Keymaster Server:
 * GET, PUT, DEL.
 *
 * @param key: A key to a YAML node. In form "key1.key2.key3" which
 * represents a hierarchy of YAML nodes on the Keymaster.
 *
 * @param val: A new value for the node pointet to by 'key'.
 *
 * @param flag: A flag regulating the operation of PUT: if 'create'
 * PUT will create a new node at the specified key if one doesn't
 * already exist. If "", it will return an error if the key does not exist.
 *
 */

yaml_result Keymaster::_call_keymaster(string cmd, string key, string val, string flag)
{
    string response;
    yaml_result yr;
    ThreadLock<Mutex> lck(_shared_lock);
    ostringstream msg;
    
    try
    {
        msg << "Keymaster: Failed to " << cmd << " key '" << key;

        lck.lock();
        shared_ptr<zmq::socket_t> km = _keymaster_socket();
        // always send a command
        z_send(*km, cmd, ZMQ_SNDMORE, KM_TIMEOUT);
        // always send a key
        z_send(*km, key, val.empty() ? 0: ZMQ_SNDMORE, KM_TIMEOUT);

        if (!val.empty())
        {
            z_send(*km, val, flag.empty() ? 0 : ZMQ_SNDMORE, KM_TIMEOUT);
        }

        if (!flag.empty())
        {
            z_send(*km, flag, 0, KM_TIMEOUT);
        }
        
        // use a reasonable time-out, in case Keymaster is gone.
        z_recv(*km, response, KM_TIMEOUT);

        yr.from_yaml_node(YAML::Load(response));
        _r = yr;
        return yr;
    }
    catch (YAML::Exception e)
    {
        msg << e.what();
        yr.err = msg.str();
        yr.result = false;
        _r = yr;
        return yr;
    }
    catch (MatrixException e)
    {
        _handle_keymaster_server_exception();
        msg << e.what();
        yr.err = msg.str();
        yr.result = false;
        _r = yr;
        return yr;
    }
    catch (zmq::error_t e)
    {
        msg << e.what();
        yr.err = msg.str();
        yr.result = false;
        _r = yr;
        return yr;
    }
    catch (std::exception e)
    {
        msg << e.what();
        yr.err = msg.str();
        yr.result = false;
        _r = yr;
        return yr;
    }

}

/**
 * Closes the socket to deal with problems such as the Keymaster
 * server disappearing. Since the socket is a ZMQ_REQ socket, sending
 * without being able to receive puts the socket into a state in which
 * it cannot resend. The shared pointer is reset, so that the
 * companion function `_keymaster_socket()` knows to create a new one
 * and reconnect.
 *
 */

void Keymaster::_handle_keymaster_server_exception()
{
    int zero = 0;

    _km_->setsockopt(ZMQ_LINGER, &zero, sizeof zero);
    _km_->close();
    _km_.reset();
}

/**
 * Attempts to return a valid zmq::socket_t connected to the keymaster
 * server. If the function is unable to connect the socket it throws a
 * zmq::error_t. This handles the potential case of the keymaster
 * going away before this client does. In that case,
 * `_handle_keymaster_server_exception()` closes the socket and resets
 * the shared pointer prior to `get()`, `put()` and `del()` throwing
 * an exception. Instead of using the `_km_` shared pointer directly,
 * `get()`, `put()` and `del()` all call this function to obtain a
 * share of this pointer to the socket; if it was previously closed
 * and reset, this will construct a new one and attempt to reconnect
 * it.
 *
 * @return std::shared_ptr<zmq::socket_t>, which will point to a
 * socket connected to the Keymaster server.
 *
 */

shared_ptr<zmq::socket_t> Keymaster::_keymaster_socket()
{
    if (_km_.get())
    {
        return _km_;
    }

    _km_.reset(new zmq::socket_t(ZMQContext::Instance()->get_context(), ZMQ_REQ));
    _km_->connect(_km_url.c_str());
    return _km_;
}

/**
 * Returns a YAML::Node corresponding to the keychain 'key'.
 *
 * example:
 *
 *      Keymaster km("inproc://keymaster");
 *      YAML::Node n = km.get("components.nettask.source.URLs");
 *
 * @param key: The keychain. Keychains are a sequence of keys separated
 * by periods (".") which will specify a path to a value in the
 * keystore.
 *
 * @return A YAML::Node corresponding to the keychain.
 *
 */

YAML::Node Keymaster::get(std::string key)
{
    yaml_result yr;

    if (!get(key, yr))
    {
        throw KeymasterException(yr.err);
    }

    return yr.node;
}

bool Keymaster::get(std::string key, yaml_result &yr)
{
    string cmd("GET");

    yr = _call_keymaster(cmd, key);
    return yr.result;
}

/**
 * Puts a YAML::Node representing some value at the node represented by
 * the given keychain. Will optionally create new nodes if some part of
 * the keychain does not exist.
 *
 * example:
 *
 *      Keymaster km("inproc://keymaster");
 *      YAML::Node n = YAML::Load("[IPC, TCP]");
 *      km.put("components.nettask.source.URLs", n);
 *
 * Will throw a KeymasterException if the key is not found and 'create'
 * is false. Will create the new node(s) if 'create' is true.
 *
 * @param key: The keychain. Keychains are a sequence of keys separated
 * by periods (".") which will specify a path to a value in the
 * keystore.
 *
 * @param n: The new yaml node to place at the end of the keychain.
 *
 * @param create: If true, the keymaster will create a new node for 'n',
 * and any new nodes it needs in between the last good key on the chain
 * and the key corresponding to 'n'.
 *
 */

bool Keymaster::put(std::string key, YAML::Node n, bool create)
{
    string cmd("PUT"), create_flag("create");
    yaml_result yr;
    ostringstream val;

    val << n;
    yr = _call_keymaster(cmd, key, val.str(), create ? create_flag : "");
    return yr.result;
}

/**
 * Deletes the node specified by the keychain 'key' from the keymaster.
 *
 * example:
 *
 *      Keymaster km("inproc://keymaster");
 *      km.delete("components.nettask.source.ID");
 *
 * @param key: The keychain. Keychains are a sequence of keys separated
 * by periods (".") which will specify a path to a value in the
 * keystore.
 *
 */

bool Keymaster::del(std::string key)
{
    string cmd("DEL");
    yaml_result yr;
    
    yr = _call_keymaster(cmd, key);
    return yr.result;
}

/**
 * Subscribes to a key on the keymaster.
 *
 * Example usage:
 *
 *     template <typename T>
 *     struct MyCallback : public KeymasterCallbackBase
 *     {
 *         MyCallback(T v)
 *           : data(v)
 *         {}
 *
 *         TCondition<T> data;
 *
 *     private:
 *         void _call(string key, YAML::Node val)
 *         {
 *             data.signal(val.as<T>());
 *         }
 *     };
 *
 *     MyCallback<int> cb(0);
 *     km.subscribe("components.nettask.source.ID", &cb);
 *
 * @param key: the subscription key.
 *
 * @param f: A pointer to a KeymasterCallbackBase functor. This functor will
 * be called whenever the value represented by 'key' updates on the
 * keymaster. NOTE: The function does not assume ownership of this
 * object! This should be managed by the thread calling this function.
 *
 */

bool Keymaster::subscribe(string key, KeymasterCallbackBase *f)
{
    // first start the subscriber thread. If it's already running this
    // won't do anything.
    _run();

    // Next, request the subscription by posting a request to the
    // subscriber thread.
    zmq::socket_t pipe(ZMQContext::Instance()->get_context(), ZMQ_REQ);
    pipe.connect(_pipe_url.c_str());
    z_send(pipe, SUBSCRIBE, ZMQ_SNDMORE);
    z_send(pipe, key, ZMQ_SNDMORE);
    z_send(pipe, f, 0);
    int rval;
    z_recv(pipe, rval);
    return rval ? true : false;
}

/**
 * Unsubscribes to a key on the keymaster. Has no effect if the key is
 * not subscribed to.
 *
 * @param key: The key to unsubscribe to.
 *
 */

bool Keymaster::unsubscribe(string key)
{
    // request that the subscriber thread unsubscribe from 'key'
    zmq::socket_t pipe(ZMQContext::Instance()->get_context(), ZMQ_REQ);
    pipe.connect(_pipe_url.c_str());
    z_send(pipe, UNSUBSCRIBE, ZMQ_SNDMORE);
    z_send(pipe, key, 0);
    int rval;
    z_recv(pipe, rval);
    return rval ? true : false;
}

/**
 * Returns a copy of the latest yaml_result.
 *
 * @param 
 *
 * @return A yaml_result which is a copy of the latest result.
 *
 */

yaml_result Keymaster::get_last_result()
{
    ThreadLock<Mutex> lck(_shared_lock);
    lck.lock();
    return _r;
}

/**
 * Starts the subscriber thread, if it is not already running.
 *
 */

void Keymaster::_run()
{
    ThreadLock<Mutex> lck(_shared_lock);

    // If the subscriber thread is not running we will need to get the
    // keymaster publishing urls for it before we start it. We obtain
    // the publishing urls here in _run() because doing so in the
    // constructor would cause an exception to be generated if the
    // KeymasterServer is not running.
    if (!_subscriber_thread.running())
    {
        // get the keymaster publishing URLs:
        try
        {
            _km_pub_urls = get_as<vector<string> >("Keymaster.URLS.AsConfigured.Pub");
        }
        catch (KeymasterException e)
        {
            cerr << e.what() << endl
                 << "Unable to obtain the Keymaster publishing URLs. Ensure a Keymaster is running and try again."
                 << endl;
        }
    }
    
    lck.lock();

    // Check again, but this time after locking. Locking before
    // getting the subscriber urls leads to a deadlock; but if we
    // check to see if it is running, then get the URLs, then lock,
    // another thread could have already started doing this. This is
    // why we check twice. The worst that can happen is that the
    // pulbishing urls are needlesly retrieved.
    if (!_subscriber_thread.running())
    {
        if ((_subscriber_thread.start() != 0) || (!_subscriber_thread_ready.wait(true, 1000000)))
        {
            throw(runtime_error("Keymaster: unable to start subscriber thread"));
        }
    }
}

/**
 * The subscriber thread. This thread handles all the subscription
 * related activities. It receives and acts on requests to subscribe and
 * unsubscribe, and it also receives and handles the data received from
 * the keymaster's publisher. This thread is responsible for running the
 * callback functors paired with the keys, so it should be kept in mind
 * that that code executes asynchronously from client code.
 *
 */

void Keymaster::_subscriber_task()
{
    zmq::socket_t sub_sock(ZMQContext::Instance()->get_context(), ZMQ_SUB);
    zmq::socket_t pipe(ZMQContext::Instance()->get_context(), ZMQ_REP);
    vector<string>::const_iterator cvi;

    // use the URL that has the same transport as the keymaster URL
    cvi = find_if(_km_pub_urls.begin(), _km_pub_urls.end(), same_transport_p(_km_url));

    // TBF: What to do if they don't match? currently just quit.
    if (cvi == _km_pub_urls.end())
    {
        cerr << "Publisher URL transport mismatch with the keymaster" << endl;
        return;
    }

    sub_sock.connect(cvi->c_str());
    pipe.bind(_pipe_url.c_str());
    _subscriber_thread_ready.signal(true);

    // we're going to poll. We will be waiting for subscription requests
    // (via 'pipe'), and for subscription data (via 'sub_sock').
    zmq::pollitem_t items [] =
        {
            { pipe, 0, ZMQ_POLLIN, 0 },
            { sub_sock, 0, ZMQ_POLLIN, 0 }
        };

    while (1)
    {
        try
        {
            zmq::poll(&items[0], 2, -1);

            if (items[0].revents & ZMQ_POLLIN) // the control pipe
            {
                int msg;
                z_recv(pipe, msg);

                if (msg == SUBSCRIBE)
                {
                    string key;
                    KeymasterCallbackBase *f_ptr;
                    z_recv(pipe, key);
                    z_recv(pipe, f_ptr);

                    // Publisher publishes this as 'Root'. A
                    // subscription with an empty key subscribes to all keys.
                    if (key.empty())
                    {
                        key = "Root";
                    }

                    _callbacks[key] = f_ptr;
                    sub_sock.setsockopt(ZMQ_SUBSCRIBE, key.c_str(), key.length());
                    z_send(pipe, 1, 0);
                }
                else if (msg == UNSUBSCRIBE)
                {
                    string key;
                    z_recv(pipe, key);

                    if (key.empty())
                    {
                        key = "Root";
                    }

                    sub_sock.setsockopt(ZMQ_UNSUBSCRIBE, key.c_str(), key.length());

                    if (_callbacks.find(key) != _callbacks.end())
                    {
                        _callbacks.erase(key);
                    }

                    z_send(pipe, 1, 0);
                }
                else if (msg == QUIT)
                {
                    z_send(pipe, 0, 0);
                    break;
                }
            }

            // The subscribed data is handled here
            if (items[1].revents & ZMQ_POLLIN)
            {
                string key;
                vector<string> val;
                z_recv(sub_sock, key);
                z_recv_multipart(sub_sock, val);

                if (!val.empty())
                {
                    YAML::Node n = YAML::Load(val[0]);
                    map<string, KeymasterCallbackBase *>::const_iterator mci;
                    mci = _callbacks.find(key);

                    if (mci != _callbacks.end())
                    {
                        mci->second->exec(mci->first, n);
                    }
                }
            }
        }
        catch (zmq::error_t e)
        {
            cout << "Keymaster subscriber task: " << e.what() << endl;
        }
    }

    int zero = 0;
    pipe.setsockopt(ZMQ_LINGER, &zero, sizeof zero);
    pipe.close();
    zero = 0;
    sub_sock.setsockopt(ZMQ_LINGER, &zero, sizeof zero);
    sub_sock.close();
}
