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
 * @param urls: The state service urls to be used with this
 * service. This is a vector, so more than one may be specified. For
 * example, tcp, ipc, and inproc may be specified in this way:
 *
 *       vector<string> urls = {"tcp://*:9000",
 *                              "ipc://matrix.keymaster",
 *                              "inproc://matrix.keymaster"}
 *
 * The KeymasterServer also publishes values that have been changed to any
 * subscribers. These URLs are not well known however. The
 * KeymasterClient class obtains these via request, so that it can
 * then subscribe.
 *
 */

KeymasterServer::KmImpl::KmImpl(YAML::Node config)
:
    _server_thread(this, &KeymasterServer::KmImpl::server_task),
    _state_manager_thread(this, &KeymasterServer::KmImpl::state_manager_task),
    _server_thread_ready(false),
    _state_manager_thread_ready(false),
    _data_queue(1000),
    _state_task_url("inproc://state_manager_task"),
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

void KeymasterServer::KmImpl::terminate()
{
    if (_state_manager_thread.running())
    {
        zmq::socket_t sock(ZMQContext::Instance()->get_context(), ZMQ_PAIR);
        sock.connect(_state_task_url.c_str());
        z_send(sock, _state_task_quit);
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
            string s = lc + ".publisher.XXXXXX";
            _publish_service_urls.push_back(s);
        }
        else if (lc.find("inproc") != string::npos)
        {
            string s = lc + ".publisher";
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
 * valid. IPC urls given with the `transit` flag need to be of the form
 * "ipc:///tmp/<name>.XXXXXX, so that the Xs may be replace by a
 * temporary string. (see man 3 mktemp). These URLs will then all be
 * replaced by the one actually used, except the TCP one (if included)
 * being completely replaced in all cases by the string
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
        else if (cvi->find("ipc") != string::npos)
        {
            // A temporary name for IPC is needed for the same reason
            // the transient port is needed for TCP: to avoid collisions
            // if more than one keymaster is running on the same machine.
            // I (RC) am aware of the man page's exhortation to "never
            // use mktemp()".  mktemp() does exactly what is needed
            // here: generate a temporary name and let 0MQ use
            // it. mkstemp() *opens* the file as well, which is not
            // wanted. No security issues are anticipated
            // here. (possible alternative: use mkdtemp())
            if (transient)
            {
                mktemp((char *)cvi->data());
            }

            server_sock.bind(cvi->c_str());
        }
        else
        {
            // The only remaining kind of URL is the inproc type, as
            // the URLs were validated by setup_urls().
            server_sock.bind(cvi->c_str());
        }
    }
}

/**
 * This is the publisher server task.  It sits on the queue waiting
 * for something to be published until it gets a "QUIT" message.  This
 * consists of putting a null pointer on the state queue.
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
            vector<string> keys;
            boost::split(keys, dp.key, boost::is_any_of("."));

            // Publish "Root" if there is no key
            if (keys.empty())
            {
                ostringstream yr;
                yr << dp.node;
                z_send(data_publisher, string("Root"), ZMQ_SNDMORE);
                z_send(data_publisher, yr.str());
            }
            else
            {
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
                        z_send(data_publisher, yr.str());
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
        // bind to all state server URLs
        bind_server(state_sock, _state_service_urls, false);
        put_yaml_val(_root_node, "KeymasterServer.URLS", _state_service_urls, true);
    }
    catch (zmq::error_t e)
    {
        cerr << "Error in state manager thread: " << e.what() << endl
             << "Exiting state thread." << endl;
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
		    z_send(state_sock, "I'm not dead yet!");
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
                            yaml_result r(true, _root_node, "Root");
                            rval << r;
                        }
                        else
                        {
                            yaml_result r = get_yaml_node(_root_node, keychain);
                            rval << r;
                        }

                        z_send(state_sock, rval.str());
                    }
                    else
                    {
                        string msg("ERROR: Keychain expected, but not received!");
                        z_send(state_sock, msg);
                    }
		}
                /////////////////// P U T ///////////////////
                else if (key.size() == 3 && key == "PUT")
                {
                    z_recv_multipart(state_sock, frame);

                    if (frame.size() > 1)
                    {
                        string keychain = frame[0];
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
                        z_send(state_sock, rval.str());

                        if (r.result)
                        {
                            publish(keychain);
                        }
                    }
                    else
                    {
                        string msg("ERROR: Keychain and value expected, but not received!");
                        z_send(state_sock, msg);
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
                        z_send(state_sock, rval.str());

                        if (r.result)
                        {
                            publish(keychain);
                        }
                    }
                    else
                    {
                        string msg("ERROR: Keychain expected, but not received!");
                        z_send(state_sock, msg);
                    }
                }
                else
                {
                    z_recv_multipart(state_sock, frame);
                    ostringstream msg;
                    msg << "Unknown request '" << key;
                    z_send(state_sock, msg.str());
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
 * both publishes data as a ZMQ PUB, but also provides a REQ/REP
 *
 * Usage involves some setup in the
 *
 *     int main(...)
 *     {
 *         // Instantiate a Zero MQ context, usually one per process.
 *         // It will be used by any 0MQ service in this process.
 *         ZMQContext::Instance();
 *         // Instantiate the 0MQ publisher, where 'component' is the
 *         // component name, and 'port' is the TCP port.
 *         pub = new KeymasterServer(component, port);
 *         ... // rest of component
 *         delete pub; // clean up
 *         ZMQContext::RemoveInstance();
 *         return 0;
 *     }
 *
 * Elsewhere in the code:
 *
 *     ...
 *     pub->publish_data(key, data_buf);
 *
 */

/**
 * KeymasterServer constructor.  Saves the URL, and starts a server
 * thread.
 *
 * @param component: The name of the publishing component.
 * @param transports: one or more of INPROC, IPC and TCP or'ed together.
 * @param portnum: The TCP port number. If not provided, will use a
 *                 transient port.
 * @param keymaster: If provided, the URL of the keymaster.
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
 * Signals the server thread that we're done, waiting for it to exit
 * on its own.
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
 *******************************************************************/

/**
 * The Keymaster client constructor makes a connection to the specified
 * Keymaster service URL. Will throw a KeymasterException if it is
 * unable to make the connection.
 *
 * @param keymaster_url: The url for the keymaster service
 *
 */

Keymaster::Keymaster(string keymaster_url)
    :
    _km(ZMQContext::Instance()->get_context(), ZMQ_REQ),
    _pipe_url("inproc://keymaster_subscriber_control"),
    _subscriber_thread(this, &Keymaster::_subscriber_task),
    _subscriber_thread_ready(false)
{
    try
    {
        _km.connect(keymaster_url.c_str());
        _km_url = keymaster_url;
    }
    catch (zmq::error_t e)
    {
        ostringstream msg;
        msg << "unable to open a connection to " << keymaster_url;
        throw(KeymasterException(msg.str()));
    }
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
        z_send(ctrl, string("QUIT"));
        int rval;
        z_recv(ctrl, rval);
        _subscriber_thread.stop_without_cancel();
        ctrl.close();
    }

    _km.setsockopt(ZMQ_LINGER, &zero, sizeof zero);
    _km.close();
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
    string cmd("GET"), response;
    vector<string> frames;

    z_send(_km, cmd, ZMQ_SNDMORE);
    z_send(_km, key);
    z_recv(_km, response);
    z_recv_multipart(_km, frames);

    _r.from_yaml_node(YAML::Load(response));

    if (_r.result == false)
    {
        ostringstream msg;
        msg << "Failed to get key " << key << " from keymaster. Keymaster says: " << _r.err;
        throw(KeymasterException(msg.str()));
    }

    return _r.node;
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

void Keymaster::put(std::string key, YAML::Node n, bool create)
{
    string cmd("PUT"), create_flag("create"), response;
    ostringstream msg;
    vector<string> frames;

    z_send(_km, cmd, ZMQ_SNDMORE);
    z_send(_km, key, ZMQ_SNDMORE);
    msg << n;

    if (create)
    {
        z_send(_km, msg.str(), ZMQ_SNDMORE);
        z_send(_km, create_flag);
    }
    else
    {
        z_send(_km, msg.str());
    }

    z_recv(_km, response);
    z_recv_multipart(_km, frames);

    _r.from_yaml_node(YAML::Load(response));

    if (_r.result == false)
    {
        ostringstream msg;
        msg << "Failed to put new value for key " << key
            << " to the keymaster. Keymaster says: " << _r.err;
        throw(KeymasterException(msg.str()));
    }
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

void Keymaster::del(std::string key)
{
    string cmd("DEL"), response;
    vector<string> frames;

    z_send(_km, cmd, ZMQ_SNDMORE);
    z_send(_km, key);
    z_recv(_km, response);
    z_recv_multipart(_km, frames);

    _r.from_yaml_node(YAML::Load(response));

    if (_r.result == false)
    {
        ostringstream msg;
        msg << "Failed to delete key " << key << " from keymaster. Keymaster says: " << _r.err;
        throw(KeymasterException(msg.str()));
    }
}

void Keymaster::subscribe(string key, KeymasterCallback *f)
{
    // first start the subscriber thread. If it's already running this
    // won't do anything.

    _run();

    zmq::socket_t pipe(ZMQContext::Instance()->get_context(), ZMQ_REQ);
    pipe.connect(_pipe_url.c_str());
    z_send(pipe, string("SUBSCRIBE"), ZMQ_SNDMORE);
    z_send(pipe, key, ZMQ_SNDMORE);
    z_send(pipe, f);
    int rval;
    z_recv(pipe, rval);
}


void Keymaster::unsubscribe(string key)
{
    zmq::socket_t pipe(ZMQContext::Instance()->get_context(), ZMQ_REQ);
    pipe.connect(_pipe_url.c_str());
    z_send(pipe, string("UNSUBSCRIBE"), ZMQ_SNDMORE);
    z_send(pipe, key);
    int rval;
    z_recv(pipe, rval);
}

void Keymaster::_run()
{
    if (!_subscriber_thread.running())
    {
        if ((_subscriber_thread.start() != 0) || (!_subscriber_thread_ready.wait(true, 1000000)))
        {
            throw(runtime_error("Keymaster: unable to start subscriber thread"));
        }
    }
}



void Keymaster::_subscriber_task()
{
    zmq::socket_t sub_sock(ZMQContext::Instance()->get_context(), ZMQ_SUB);
    zmq::socket_t pipe(ZMQContext::Instance()->get_context(), ZMQ_REP);
    vector<string>::const_iterator cvi;
    vector<string> km_pub_urls;

    // get the keymaster publishing URLs:
    try
    {
        km_pub_urls = get_as<vector<string> >("Keymaster.URLS.AsConfigured.Pub");
    }
    catch (KeymasterException e)
    {
        cerr << "Unable to obtain the Keymaster publishing URLs. Ensure a Keymaster is running and try again."
             << endl;
        return;
    }

    // use the URL that has the same transport as the keymaster URL
    cvi = find_if(km_pub_urls.begin(), km_pub_urls.end(), same_transport_p(_km_url));

    // TBF: What to do if they don't match? currently just quit.
    if (cvi == km_pub_urls.end())
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
                string msg;
                z_recv(pipe, msg);

                if (msg == "SUBSCRIBE")
                {
                    string key;
                    KeymasterCallback *f_ptr;
                    z_recv(pipe, key);
                    z_recv(pipe, f_ptr);

                    _callbacks[key] = f_ptr;
                    sub_sock.setsockopt(ZMQ_SUBSCRIBE, key.c_str(), key.length());
                    z_send(pipe, 1);
                }
                else if (msg == "UNSUBSCRIBE")
                {
                    string key;
                    z_recv(pipe, key);
                    sub_sock.setsockopt(ZMQ_UNSUBSCRIBE, key.c_str(), key.length());
                    _callbacks.erase(key);
                    z_send(pipe, 1);
                }
                else if (msg == "QUIT")
                {
                    z_send(pipe, 0);
                    break;
                }
            }

            if (items[1].revents & ZMQ_POLLIN)
            {
                string key;
                vector<string> val;
                z_recv(sub_sock, key);
                z_recv_multipart(sub_sock, val);

                if (!val.empty())
                {
                    YAML::Node n = YAML::Load(val[0]);
                    map<string, KeymasterCallback *>::const_iterator mci;
                    mci = _callbacks.find(key);

                    if (mci != _callbacks.end())
                    {
                        mci->second->exec(n);
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
