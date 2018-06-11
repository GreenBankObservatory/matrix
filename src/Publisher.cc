/*******************************************************************
 *  Publisher.cc - implementation of the bank manager's data
 *  publishing service.  The publisher consists of a thread running
 *  the server and polling an incoming IPC socket.  This incoming IPC
 *  socket brings data from the rest of the manager to the publisher.
 *  The publisher then publishes the data.
 *
 *  Copyright (C) <date> Associated Universities, Inc. Washington DC, USA.
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

#include "matrix/Publisher.h"
#include "matrix/ZMQContext.h"
#include "matrix/ThreadLock.h"
#include "matrix/tsemfifo.h"
#include "matrix/Thread.h"
#include "matrix/zmq_util.h"
#include "matrix/matrix_util.h"
#include "matrix/netUtils.h"

#include <string>
#include <cstring>
#include <sstream>
#include <map>
#include <vector>
#include <iostream>

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include <boost/circular_buffer.hpp>
#include <boost/algorithm/string.hpp>

using namespace std;
using namespace mxutils;

#define DEBUG

/**
 * \class PubImpl is the private implementation of the Publisher class.
 *
 */

struct Publisher::PubImpl
{
    PubImpl(vector<vector<string> > urls);
    ~PubImpl();

    struct data_package
    {
        string key;
        string data;
    };

    enum
    {
        PUBLISH = 0,
        STATE,
        NUM_INTERFACES
    };

    void server_task();
    void state_manager_task();
    bool publish(string key, string data);
    void save_val(string key, string data);
    void send_snapshot(zmq::socket_t &s, string &key);
    void send_list_of_keys(zmq::socket_t &s);
    vector<vector<string> > get_urls();

    Thread<PubImpl> _server_thread;
    Thread<PubImpl> _state_manager_thread;
    TCondition<bool> _server_thread_ready;
    TCondition<bool> _state_manager_thread_ready;

    bool _state_manager_done;
    tsemfifo<data_package> _data_queue;
    Mutex _cache_lock;
    string _state_task_url;
    bool _state_task_quit;
    string _hostname;


    vector<string> _state_service_urls;
    vector<string> _publish_service_urls;
    vector<vector<string> > _given_urns;

    typedef map<string, string> pub_map;
    pub_map _kv_cache;
    zmq::context_t &_ctx;
};

/**
 * Constructor of the implementation class of the publisher.  The
 * implementation class handles all the details; the Publisher class
 * merely provides the external interface.
 *
 * @param urns: The desired URNs, as a vector of vector of strings. If
 * only the transport is given, ephemeral URLs will be generated. There
 * are two sets of urns; the first element of the outer vector is for
 * the state server, the second for the pub server.
 *
 * @param portnum: The optional port.  If 0, or not given, the service
 * will choose its own ports, which is the standard operating mode if
 * the service is some sort of directory service.
 *
 */

Publisher::PubImpl::PubImpl(vector<vector<string> > urns)
:
    _server_thread(this, &Publisher::PubImpl::server_task),
    _state_manager_thread(this, &Publisher::PubImpl::state_manager_task),
    _server_thread_ready(false),
    _state_manager_thread_ready(false),
    _state_manager_done(false),
    _data_queue(1000),
    _state_task_url(string("inproc://") + gen_random_string(20)),
    _state_task_quit(true),
    _given_urls(urls),
    _ctx(ZMQContext::Instance()->get_context())

{
    int i = 0;

    // process the urns. There must be two sets. All urns provided must
    // be valid (see docs for 'process_zmq_urn')

    if (urns.size() != 2)
    {
        throw PublisherException("need two sets of urns: one for the state service, "
                                 "the other for the publishing service.");
    }

    transform(urns[0].begin(), urns(0).end(), _state_service_urls.begin(), &process_zmq_urn);
    transform(urns[1].begin(), urns(1).end(), _publish_service_urls.begin(), &process_zmq_urn);
    auto str_not_empty = bind(not_equal_to<string>(), _1, string());

    if (!all_of(_state_service_urls.begin(), _state_service_urls.end(), str_not_empty))
    {
        throw PublisherException("Cannot use one or more of the following transports", urns[0]);
    }

    if (!all_of(_publish_service_urls.begin(), _publish_service_urls.end(), str_not_empty))
    {
        throw PublisherException("Cannot use one or more of the following transports", urns[1]);
    }

    if (!getCanonicalHostname(_hostname))
    {
        string error = string("Unable to obtain canonical hostname: ") + strerror(errno);
        throw PublisherException(error);
    }

    if (!_server_thread.running())
    {
        if (_server_thread.start() != 0)
        {
            string error("Publisher data task was not started.");
            throw PublisherException(error);
        }
    }

    if (!_state_manager_thread.running())
    {
        if (_state_manager_thread.start() != 0)
        {
            string error("Publisher state manager task was not started.");
            throw PublisherException(error);
        }
    }

    _server_thread_ready.wait(true, 1000000);
    _state_manager_thread_ready.wait(true, 1000000);
}

/**
 * Signals the server thread that we're done, waiting for it to exit
 * on its own.
 *
 */

Publisher::PubImpl::~PubImpl()

{
    zmq::socket_t sock(_ctx, ZMQ_PAIR);
    sock.connect(_state_task_url.c_str());
    z_send(sock, _state_task_quit);
    sock.close();
    _data_queue.release();
    _server_thread.stop_without_cancel();
    _state_manager_thread.stop_without_cancel();
}

/**
 * Returns the URLs bound to the services.
 *
 * @return A vector of two vectors of strings. The first element is the
 * state service URL set, the second is the publisher service URL set.
 *
 */

vector<vector<string> > Publisher::PubImpl::get_urls()
{
    vector<vector<string> > urls;
    urls.push_back(_state_service_urls);
    urls.push_back(_publish_service_urls);
    return urls;
}


/**
 * This is the publisher server task.  It sits on the queue waiting
 * for something to be published until it gets a "QUIT" message.  This
 * consists of putting a null pointer on the state queue.
 *
 */

void Publisher::PubImpl::server_task()

{
    data_package dp;
    zmq::socket_t data_publisher(_ctx, ZMQ_PUB);
    string tcp_url;

    try
    {
        vector<string>::iterator *urn;

        for (urn = _publish_service_urls.begin(); urn != _publish_service_urls.end(); ++urn)
        {
            boost::regex p_tcp("^tcp"), p_inproc("^inproc"), p_ipc("^ipc"), p_xs("X+$");
            boost::smatch result;

            // bind using tcp. If port is not given (port == 0), then use ephemeral port.
            if (boost::regex_search(*urn, p_tcp, result))
            {
                int port_used;

                // ephem port requested? ("tcp://*:XXXXX")
                if (boost::regex_search(*urn, p_xs, result))
                {
                    port_used = zmq_ephemeral_bind(data_publisher, "tcp://*:*", 1000);
                }
                else
                {
                    data_publisher.bind(urn->c_str());
                    vector<string> parts;
                    boost::split(parts, *urn, boost::ist_any_of(":"));
                    port_used = convert<int>(parts[2]);
                }

                // transmogrify the tcp urn to the actual urn needed for
                // a clientto access the service:
                // 'tcp://<canonical_hostname>:<port>
                ostringstream tcp_url;
                tcp_url << "tcp://" << _hostname << ":" << port_used;
                *urn = tcp_url.str();
            }

            // bind using IPC or INPROC:
            if (boost::regex_search(*urn, p_ipc, result) || boost::regex_search(*urn, p_inproc, result))
            {
                // these are already in a form the client can use.
                data_publisher.bind(urn->c_str());
            }
        }
    }
    catch (zmq::error_t &e)
    {
        cerr << "Error in publisher thread: " << e.what() << endl
             << "Exiting publishing thread." << endl;
        return;
    }

    _server_thread_ready.signal(true); // Allow constructor to move on

    while (_data_queue.get(dp))
    {
        // Publish data.  A copy of the data is also saved for use by
        // the snapshot server.
        //
        // A note on copying.  It is tempting to use the zero-copy
        // feature of ZMQ to avoid this extra copying:
        //
        // zmq::message_t payload(ser_data.data(), ser_data.size(), NULL);
        // data_publisher.send(payload) /* zero copy */
        //
        // The problem with this is that there is no way to guarantee
        // the lifetime of the data owned by 'ser_data' and later
        // owned by a string contained in the snapshot map.  If OMQ
        // has to queue stuff up, the next data for the same key might
        // come along before the previous one was sent.  When it is
        // saved by 'save_val', the previous buffer will then be
        // deallocated by the string's destructor, and when 0MQ
        // finally gets around to sending it, it has a bum pointer and
        // cores. To summarize, 0MQ's zero-copy feature is fire and
        // forget (you allocate the memory, pass it an a deallocation
        // function to zmq::message_t, then forget about the allocated
        // memory.)  But we can't forget the data if we wish to keep
        // the snapshot feature.

        try
        {
            z_send(data_publisher, dp.key, ZMQ_SNDMORE);
            zmq::message_t payload(dp.data.size());
            memcpy(payload.data(), dp.data.data(), payload.size());
            data_publisher.send(payload);
            // Record this key-value pair for late joiners:
            save_val(dp.key, dp.data);
        }
        catch (zmq::error_t &e)
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

void Publisher::PubImpl::state_manager_task()

{
    zmq::context_t &ctx = ZMQContext::Instance()->get_context();
    zmq::socket_t initial_state(ctx, ZMQ_REP);
    zmq::socket_t pipe(ctx, ZMQ_PAIR);  // mostly to tell this task to
                                        // go away
    _state_service_urls.clear();

    try
    {
        pipe.bind(_state_task_url.c_str());

        if ((_transports | TCP) == TCP)
        {
            if (_port)
            {
                ostringstream url;
                url << "tcp://*:" << _port + 1;
                string init_url = url.str();
                initial_state.bind(init_url.c_str());
                _state_port_used = _port + 1;
            }
            else
            {
                _state_port_used = zmq_ephemeral_bind(initial_state, "tcp://*:*", 1000);
            }

            ostringstream tcp_url;
            tcp_url << "tcp://" << _hostname << ":" << _state_port_used;
            _state_service_urls.push_back(tcp_url.str());
        }

        // Bind IPC and INPROC. See commentary in 'server_task()' for more.

        if ((_transports | IPC) == IPC)
        {
            string ipc_url = string("ipc:///tmp/") + gen_random_string(20);
            initial_state.bind(ipc_url.c_str());
            _state_service_urls.push_back(ipc_url);
        }

        if ((_transports | INPROC) == INPROC)
        {
            string inproc_url = string("inproc://") + gen_random_string(20);
            initial_state.bind(inproc_url.c_str());
            _state_service_urls.push_back(inproc_url);
        }
    }
    catch (zmq::error_t &e)
    {
        cerr << "Error in state manager thread: " << e.what() << endl
             << "Exiting thread." << endl;
        return;
    }

    zmq::pollitem_t items [] =
        {
            { pipe, 0, ZMQ_POLLIN, 0 },
            { initial_state, 0, ZMQ_POLLIN, 0 }
        };

    _state_manager_thread_ready.signal(true); // allow constructor to
                                              // move on
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
		z_recv(initial_state, key);

		// Determine the request.  Currently requests may
		// be either a "ping" (just to see if the service
		// is alive); a "LIST" to get information on all
		// samplers and parameters published.  If none of
		// the above, the request is assumed to be a key
		// to a published item.

		if (key.size() == 4 && key == "ping")
		{
		    // reply with something
		    z_send(initial_state, "I'm not dead yet!");
		}
		else if (key.size() == 4 && key == "LIST")
		{
		    // provide list of stuff
		    send_list_of_keys(initial_state);
		}
		else  // assume everything else is a subcription key
		{
		    send_snapshot(initial_state, key);
		}
	    }
        }
        catch (zmq::error_t &e)
        {
            cerr << "State manager task, main loop: " << e.what() << endl;
        }
    }

    int zero = 0;
    initial_state.setsockopt(ZMQ_LINGER, &zero, sizeof zero);
    initial_state.close();
}

/**
 * This function saves a published value in its key's circular buffer.
 * This is for late/initial joiners.  In the case of samplers the
 * circular buffer is more than 1 in size, thus retaining a history of
 * that sampler in accordance to what that Sampler class' circular
 * buffer size was set to.
 *
 * Here is an example of what a client would have to do to get the
 * values from the circular buffer upon subcribing to the value:
 *
 *   1) Client subscribes to "Undulator.Undulator:S:foo" over a
 *      ZMQ_SUB socket, _but does not receive yet_.
 *   2) Over a separate ZMQ_REQ socket, client requests a snapshot of
 *      the circular buffer for "Undulator.Undulator:S:foo"
 *   3) Server receives request, on a separate ZMQ_REP socket set
 *      up for the purpose, and transmits to the client the contents
 *      of the circular buffer.
 *   4) Client receives the snapshot and reconstructs the circular
 *      buffer on its end

 *   5) Client now starts processing the ZMQ_SUB socket.  It uses the
 *      TimeStamps that accompany each sampler to decide whether to
 *      keep any values received (and buffered by 0MQ) during steps
 *      2-4 and merge them in its circular buffer, or whether to
 *      discard them.
 *
 * The client of course need not do all of this; it may merely
 * subscribe to the sampler and patiently wait for the next value, and
 * do whatever it sees fit with it.  This facility exists for those
 * clients who wish to reconstruct the circular buffer at their end,
 * or at the least immediately receive the last value posted.
 *
 * @param key: The key in the key/value pair
 * @param value: the value part. The string is used solely as a buffer
 * @param circ_buf_size: initial size for key's history circular buffer
 *
 */

void Publisher::PubImpl::save_val(string key, string value)
{
    ThreadLock<Mutex> l(_cache_lock);

    l.lock();
    _kv_cache[key] = value;
}

/**
 * Given a socket, an address for the requestor, and a key, sends off
 * all the values stored in the circular buffer for that key.
 *
 * @param s: the socket
 * @param key: The key to the desired values
 *
 */

void Publisher::PubImpl::send_snapshot(zmq::socket_t &s, string &key)
{
    map<string, string>::iterator it;
    ThreadLock<Mutex> l(_cache_lock);

    l.lock();
    it = _kv_cache.find(key);

    if (it == _kv_cache.end())
    {
        l.unlock();
        z_send(s, "E_NOKEY");
    }
    else
    {
        string &val = it->second;
        z_send(s, key, ZMQ_SNDMORE);
        z_send(s, val);
    }
}

/**
 * Given a component name and socket for the requestor sends off
 * all the keys being served for this component.
 *
 * @param s: the socket that will return the values to the entity
 * requesting the keys.
 * @param component: Names the component publishing the keys
 *
 */

void Publisher::PubImpl::send_list_of_keys(zmq::socket_t &s)
{
    pub_map::iterator it;
    ThreadLock<Mutex> l(_cache_lock);

    l.lock();

    for (it = _kv_cache.begin(); it != _kv_cache.end(); ++it)
    {
        z_send(s, it->first, ZMQ_SNDMORE);
    }

    z_send(s, "END");
}

/**
 * This is the Publisher's Data publishing facility.  It is a private
 * function that does all the work preparing data for publication.
 * The data and metadata are copied into a 'data_package' object and
 * posted on the publication semaphore queue.  Public inline functions
 * such as 'publish_data()' call this with the appropriate parameters
 * set.  This 'publish' function assumes the data is not Sampler or
 * Parameter data.
 *
 * @param key: the data key
 * @param data: the data buffer
 * @param circ_buf_size: the desired size of the circular buffer.
 *
 * @return true if the data was succesfuly placed in the publication
 * queue, false otherwise.
 *
 */

bool Publisher::PubImpl::publish(string key, string data)
{
    data_package dp;

    dp.key = key;
    dp.data = data;

    if (!_data_queue.try_put(dp))
    {
        return false;
    }

    return true;
}

/**
 * \class Publisher
 *
 * This is a class that provides a 0MQ publishing facility. Publisher
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
 *         pub = new Publisher(component, port);
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
 * Publisher constructor.  Saves the URL, and starts a server
 * thread.
 *
 * @param component: The name of the publishing component.
 * @param transports: one or more of INPROC, IPC and TCP or'ed together.
 * @param portnum: The TCP port number. If not provided, will use a
 *                 transient port.
 * @param keymaster: If provided, the URL of the keymaster.
 *
 */

Publisher::Publisher(string keymaster, string key)
    :
    _impl(new Publisher::PubImpl(keymaster, key))

{
}

/**
 * Signals the server thread that we're done, waiting for it to exit
 * on its own.
 *
 */

Publisher::~Publisher()

{
    _impl.reset();
}

/**
 * Publishes a buffer of data using a given key.
 *
 * @param key: The publishing key for the data.
 * @param data: The data buffer.
 *
 * @return true if the data was succesfuly placed in the publication
 * queue, false otherwise.
 *
 */

bool Publisher::publish_data(string key, string data)
{
    return _impl->publish(key, data);
}

/**
 * Returns the actually configured URLs for the publishing
 * service. There are two sets of URLs: A set for the state request
 * server, which provides snapshots of the last published data for a
 * given key, and a set for the publisher itself.
 *
 * @param
 *
 * @return A vector consisting of two elements, each itself a vector of
 * strings, the URLs. The first element of the top-level vector is the
 * vector of state service URLs, and the second element is the vector of
 * publisher URLs. Each set consists of 1 to 3 URLs, depending on the
 * transports used (tcp, inproc, ipc).
 *
 */

vector<vector<string> > Publisher::get_urls()
{
    return _impl->get_urls();
}
