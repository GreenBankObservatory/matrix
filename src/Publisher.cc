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

#include "Publisher.h"
#include "ZMQContext.h"
#include "ThreadLock.h"
#include "tsemfifo.h"
#include "Thread.h"
#include "zmq_util.h"
#include "netUtils.h"

#include <string>
#include <cstring>
#include <sstream>
#include <map>
#include <vector>
#include <iostream>

#include <stdlib.h>
#include <sys/stat.h>

#include <boost/circular_buffer.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/shared_ptr.hpp>

using namespace std;
using namespace mxutils;

#define DEBUG

/******************************************************************//**
 * PubImpl is the private implementation of the Publisher class.
 *
 *******************************************************************/

struct Publisher::PubImpl
{
    PubImpl(string component, int transports, int portnum, string keymaster);
    ~PubImpl();

    struct data_package
    {
        std::string key;
        std::string data;
        int circ_buf_size;
    };

    enum
    {
        PUBLISH = 0,
        STATE,
        NUM_INTERFACES
    };

    void server_task();
    void state_manager_task();
    bool publish(std::string &key, std::string &data, int circ_buf_size);
    void save_val(std::string key, std::string data, int circ_buf_size);
    void send_snapshot(zmq::socket_t &s, std::string &key);
    void send_list_of_keys(zmq::socket_t &s, std::string component);
    void register_service(vector<string> &urls, int interface);

    Thread<PubImpl> _server_thread;
    Thread<PubImpl> _state_manager_thread;
    TCondition<bool> _server_thread_ready;
    TCondition<bool> _state_manager_thread_ready;

    std::string _component;
    std::string _keymaster;
    int _port;
    int _transports;
    int _state_port_used;
    int _pub_port_used;
    bool _state_manager_done;
    tsemfifo<data_package> _data_queue;
    Mutex _cache_lock;
    std::string _state_task_url;
    bool _state_task_quit;
    std::string _ns_reg_url;
    std::string _ns_pub_url;
    std::string _hostname;


    // The service URLs. Each interface (STATE or PUBLISH) may have
    // multiple URLs (tcp, udp, inproc, ipc). Record them for
    // (re)registration:
    std::vector<std::string> _state_service_urls;
    std::vector<std::string> _publish_service_url;

    // the idea here is to have a map of published values, with the
    // key = sampler/parameter name.  The map consists of a circular
    // buffer of each value; so there is a key, and 1 or more values,
    // depending on the circular buffer size.  Parameters will always
    // be 1, samplers 1 to n, depending on what the Sampler class is
    // constructed with. (NOTE: Linux Sampler classes are set to 1)
    // The data in the circular buffer will consist of a string of
    // bytes, which may already be in a wire-serialized format.  Thus
    // this map can be used to store anything that was published,
    // without knowledge of how it was serialized.
    typedef std::map<std::string, boost::circular_buffer<string> > pub_map;
    pub_map _kv_cache;
    zmq::context_t &_ctx;
};

/*****************************************************************//**
 * Constructor of the implementation class of the publisher.  The
 * implementation class handles all the details; the Publisher class
 * merely provides the external interface.
 *
 * The component is provided only for the purposes of establishing the
 * endpoint URL for the client.  The port is also provided for that
 * reason; however, in the declaration of the Publisher constructor, it
 * is defaulted to 0.  If the port is 0, then the implementation will
 * find ephemeral ports to use for the service.  In either case, it will
 * attempt to report the host name and ports to the directory name
 * server.
 *
 * @param component: The component name.  Used to register the service
 * with the keymaster.
 * @param transports: Any combination of the Publisher::INPROC,
 * Publisher::IPC, Publisher::TCP enums defined in Publisher.h. For
 * example, Publisher::IPC | Publisher::TCP provides those two
 * interfaces.
 * @param portnum: The optional port.  If 0, or not given, the service
 * will choose its own ports, which is the standard operating mode if
 * the service is using a name server.
 * @param keymaster: The URL to the keymaster. If not given defaults to
 * "".
 *
 *******************************************************************/

Publisher::PubImpl::PubImpl(string component, int transports, int portnum, string keymaster)
:
    _server_thread(this, &Publisher::PubImpl::server_task),
    _state_manager_thread(this, &Publisher::PubImpl::state_manager_task),
    _server_thread_ready(false),
    _state_manager_thread_ready(false),
    _component(component),
    _keymaster(keymaster),
    _port(portnum),
    _transports(transports),
    _state_manager_done(false),
    _data_queue(1000),
    _state_task_url("inproc://state_manager_task"),
    _state_task_quit(true),
    _ctx(ZMQContext::Instance()->get_context())
{
    int i = 0;

    if (!getCanonicalHostname(_hostname))
    {
        perror("Unable to obtain canonical hostname:");
        exit(1);
    }

    if (!_server_thread.running())
    {
        if (_server_thread.start() != 0)
        {
            cerr << "Publisher data task was not started.  "
                 << "There will be no data published."
                 << endl;
        }
    }

    if (!_state_manager_thread.running())
    {
        if (_state_manager_thread.start() != 0)
        {
            cerr << "Publisher state manager task was not started.  "
                 << "Late joiners will not get updates."
                 << endl;
        }
    }

    _server_thread_ready.wait(true, 1000000);
    _state_manager_thread_ready.wait(true, 1000000);
}

/****************************************************************//**
 * Signals the server thread that we're done, waiting for it to exit
 * on its own.
 *
 ********************************************************************/

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

/****************************************************************//**
 * Registers the URLs for the given interface for this publisher. This
 * uses the Lazy Pirate pattern to prevent indefinite blocking should
 * the Keymager not be present.
 * (http://zguide.zeromq.org/page:all#toc89) This involves making the
 * request then dropping into a poll loop with a time-out to await the
 * response. If Keymaster is not there this will fail. Eventually after
 * exhausting the retries it will give up.
 *
 * The service URLs will still be registered properly when the Keymaster
 * comes back up. In that case the Publisher will receive the up
 * message, and try again.
 *
 * @param urls: The URLs for this interface. There are up to 3 urls,
 * depending on the transports chosen.
 * @param interface: the interface, PubImpl::PUBLISH is the publisher
 * and PubImpl::STATE is the state service.
 *
 *******************************************************************/

void Publisher::PubImpl::register_service(vector<string> &urls, int interface)

{
    int REQUEST_TIMEOUT = 2500; // msecs, (> 1000!)
    int REQUEST_RETRIES = 2;    // Before we abandon
    int zero = 0;

    // try
    // {
    //     PBRegisterService regpb;
    //     regpb.set_major(device.c_str());
    //     regpb.set_minor(cit->c_str());
    //     regpb.set_interface((PBRegisterService::Interface)interface);
    //     regpb.set_host(hostname);
    //     regpb.clear_url();

    //     for (vector<string>::const_iterator vi = urls.begin(); vi != urls.end(); ++vi)
    //     {
    //         regpb.add_url(*vi);
    //     }

    //     zmq::message_t msg(regpb.ByteSize());
    //     regpb.SerializeToArray(msg.data(), regpb.ByteSize());
    //     boost::shared_ptr<zmq::socket_t> sock(new zmq::socket_t(ctx, ZMQ_REQ));
    //     sock->connect(ns_reg_url.c_str());
    //     string reply;
    //     bool success = false;
    //     int retries_left = REQUEST_RETRIES;

    //     while (!success && retries_left > 0)
    //     {
    //         sock->send(msg);
    //         bool expect_reply = true;

    //         while (expect_reply)
    //         {
    //             // Poll socket for a reply, with timeout
    //             zmq::pollitem_t items[] = { { *sock, 0, ZMQ_POLLIN, 0 } };
    //             zmq::poll (&items[0], 1, REQUEST_TIMEOUT);

    //             // If we got a reply, process it
    //             if (items[0].revents & ZMQ_POLLIN)
    //             {
    //                 // We got a reply from the server, all is well!
    //                 z_recv(*sock, reply);
    //                 success = true;
    //                 expect_reply = false;
    //             }
    //             else
    //             {
    //                 // Timed out. Should we retry again?
    //                 if (--retries_left == 0)  // no.
    //                 {
    //                     cout << "YgorDirectory seems to be offline. Will register when it comes back up"
    //                          << endl;
    //                     expect_reply = false;
    //                 }
    //                 else  // yes.
    //                 {
    //                     cout << "No response from YgorDirectory @ "
    //                          << ns_reg_url << ", retrying…" << endl;
    //                     // Get rid of old socket, it is in an
    //                     // unusable state
    //                     sock->setsockopt(ZMQ_LINGER, &zero, sizeof zero);
    //                     sock->close();
    //                     sock.reset(new zmq::socket_t(ctx, ZMQ_REQ));
    //                     sock->connect(ns_reg_url.c_str());
    //                     sock->send(msg);
    //                 }
    //             }
    //         }
    //     }

    //     if (success)
    //     {
    //         cout << "registering service." << endl
    //              << "major = " << device << endl
    //              << "minor = " << *cit << endl
    //              << "host = " << hostname << endl
    //              << "interface = " << interface << endl
    //              << "URLs:" << endl;

    //         for (vector<string>::const_iterator vi = urls.begin(); vi != urls.end(); ++vi)
    //         {
    //             cout << *vi << endl;
    //         }

    //         cout << "reply is: " << reply[0] << reply[1] << endl;

    //         if (reply == "OK")
    //         {
    //             cout << "Register: " << device << "." << *cit << " success!" << endl;
    //         }
    //         else
    //         {
    //             cout << "Register: " << device << "." << *cit << " failed." << endl;
    //         }
    //     }
    //     else
    //     {
    //         // We failed. Don't repeat this for every child if no
    //         // reply was received; it won't work for them
    //         // either. Break out of the for-loop, which takes us out
    //         // of this function.
    //         cout << "Publisher failed to register with YgorDirectory. "
    //              << "Registering will take place when YgorDirectory comes back up."
    //              << endl;
    //     }

    //     sock->setsockopt(ZMQ_LINGER, &zero, sizeof zero);
    //     sock->close();
    // }
    // catch (zmq::error_t e)
    // {
    //     cout << "register_service():" << e.what() << endl;
    // }
}

/****************************************************************//**
 * This is the publisher server task.  It sits on the queue waiting
 * for something to be published until it gets a "QUIT" message.  This
 * consists of putting a null pointer on the state queue.
 *
 *******************************************************************/

void Publisher::PubImpl::server_task()

{
    data_package dp;
    zmq::socket_t data_publisher(_ctx, ZMQ_PUB);
    string tcp_url;

    try
    {
        _publish_service_url.clear();

        // bind using tcp. If port is not given (port == 0), then use ephemeral port.

        if ((_transports | TCP) == TCP)
        {
            if (_port)
            {
                ostringstream url;
                url << "tcp://*:" << _port;
                data_publisher.bind(url.str().c_str());
                _pub_port_used = _port;
            }
            else
            {
                _pub_port_used = zmq_ephemeral_bind(data_publisher, "tcp://*:*", 1000);
            }

            ostringstream tcp_url;
            tcp_url << "tcp://" << _hostname << ":" << _pub_port_used;
            _publish_service_url.push_back(tcp_url.str());
        }

        // also bind to IPC and INPROC to give clients these
        // options. Clients should use the IPC URL returned by the
        // Directory Service, and for inproc 'inproc://<major>.<minor>'
        // For managers with many subdevices all in one process the main
        // Publisher will serve them all, so only one URL is needed per
        // transport. For this the first subdevice will be used, which
        // is normally the same name as the device.

        if ((_transports | IPC) == IPC)
        {
            string ipc_url = string("ipc:///tmp/") + _component + ".publisher.XXXXXX";

            // A temporary name for IPC is needed for the same reason
            // the transient port is needed for TCP: to avoid collisions
            // if more than one manager is running on the same machine.
            // I (RC) am aware of the man page's exhortation to "never
            // use mktemp()".  mktemp() does exactly what is needed
            // here: generate a temporary name and let 0MQ use
            // it. mkstemp() *opens* the file as well, which is not
            // wanted. No security issues are anticipated
            // here. (possible alternative: use mkdtemp())
            mktemp((char *)ipc_url.data());
            _publish_service_url.push_back(ipc_url);
            data_publisher.bind(ipc_url.c_str());
        }

        if ((_transports | INPROC) == INPROC)
        {
            string inproc_url = string("inproc://") + _component + ".publisher";
            data_publisher.bind(inproc_url.c_str());
            _publish_service_url.push_back(inproc_url);
        }
    }
    catch (zmq::error_t e)
    {
        cerr << "Error in publisher thread: " << e.what() << endl
             << "Exiting publishing thread." << endl;
        return;
    }

    // register this with the keymaster
    // register_service(_publish_service_url, PUBLISH);
    _server_thread_ready.signal(true); // Allow constructor to move on

    while (_data_queue.get(dp))
    {
        // Publish data.  A copy of the serialized data, whether it be
        // Sampler, Parameter or Data, is also saved for use by the
        // snapshot server.
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
	    save_val(dp.key, dp.data, dp.circ_buf_size);
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

/****************************************************************//**
 * This 0MQ server task implements a REPLY socket (of a REQ/REP pair),
 * whose function is to receive requests for cached data.  This is
 * useful for late joiners, especially for keys that don't change
 * frequently.
 *
 *******************************************************************/

void Publisher::PubImpl::state_manager_task()

{
    zmq::context_t &ctx = ZMQContext::Instance()->get_context();
    zmq::socket_t initial_state(ctx, ZMQ_REP);
    zmq::socket_t pipe(ctx, ZMQ_PAIR);  // mostly to tell this task to go away
    std::string ns_key = "Keymaster:SERVER_UP";

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
            string ipc_url = string("ipc:///tmp/") + _component + ".snapshot.XXXXXX";
            mktemp((char *)ipc_url.data());
            initial_state.bind(ipc_url.c_str());
            _state_service_urls.push_back(ipc_url);
        }

        if ((_transports | INPROC) == INPROC)
        {
            string inproc_url = string("inproc://") + _component + ".snapshot";
            initial_state.bind(inproc_url.c_str());
            _state_service_urls.push_back(inproc_url);
        }
    }
    catch (zmq::error_t e)
    {
        cerr << "Error in state manager thread: " << e.what() << endl
             << "Exiting thread." << endl;
        return;
    }

    register_service(_state_service_urls, STATE);

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
		    string component;
		    z_recv(initial_state, component);
		    // provide list of stuff
		    send_list_of_keys(initial_state, component);
		}
		else  // assume everything else is a subcription key
		{
		    send_snapshot(initial_state, key);
		}
	    }
        }
        catch (zmq::error_t e)
        {
            cerr << "State manager task, main loop: " << e.what() << endl;
        }
    }

    int zero = 0;
    initial_state.setsockopt(ZMQ_LINGER, &zero, sizeof zero);
    initial_state.close();
}

/****************************************************************//**
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
 *******************************************************************/

void Publisher::PubImpl::save_val(std::string key, std::string value, int circ_buf_size)
{
    map<string, boost::circular_buffer<std::string> >::iterator it;
    ThreadLock<Mutex> l(_cache_lock);


    l.lock();
    // first check to see if this is the initial value published for
    // this key:
    it = _kv_cache.find(key);

    if (it == _kv_cache.end())
    {
        _kv_cache[key].resize(circ_buf_size);
    }

    _kv_cache[key].push_back(value);
}

/****************************************************************//**
 * Given a socket, an address for the requestor, and a key, sends off
 * all the values stored in the circular buffer for that key.
 *
 * @param s: the socket
 * @param key: The key to the desired values
 *
 *******************************************************************/

void Publisher::PubImpl::send_snapshot(zmq::socket_t &s, std::string &key)
{
    map<string, boost::circular_buffer<string> >::iterator it;
    ThreadLock<Mutex> l(_cache_lock);


    l.lock();
    it = _kv_cache.find(key);

    if (it == _kv_cache.end())
    {
        l.unlock();       // don't need that no mo'
        z_send(s, "E_NOKEY");
    }
    else
    {
        boost::circular_buffer<string> &cb = it->second;
        z_send(s, key, ZMQ_SNDMORE);

        for (size_t i = 0; i != cb.size(); ++i)
        {
            zmq::message_t dat_msg(cb[i].size());
	    memcpy(dat_msg.data(), cb[i].data(), cb[i].size());

            if (i < (cb.size() - 1))
            {
                s.send(dat_msg, ZMQ_SNDMORE);
            }
            else
            {
                // last one, signal end of message sequence.
                s.send(dat_msg);
            }
        }
    }
}

/****************************************************************//**
 * Given a component name and socket for the requestor sends off
 * all the keys being served for this component.
 *
 * @param s: the socket that will return the values to the entity
 * requesting the keys.
 * @param component: Names the component publishing the keys
 *
 *******************************************************************/

void Publisher::PubImpl::send_list_of_keys(zmq::socket_t &s, std::string component)
{
    pub_map::iterator it;
    ThreadLock<Mutex> l(_cache_lock);

    l.lock();

    for (it = _kv_cache.begin(); it != _kv_cache.end(); ++it)
    {
        if (it->first == component)
        {
            z_send(s, it->first, ZMQ_SNDMORE);
        }
    }

    z_send(s, "END");
}

/****************************************************************//**
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
 *******************************************************************/

bool Publisher::PubImpl::publish(std::string &key,
                                 std::string &data, int circ_buf_size)
{
    data_package dp;

    dp.key = key;
    dp.data = data;
    dp.circ_buf_size = circ_buf_size;

    if (!_data_queue.try_put(dp))
    {
        return false;
    }

    return true;
}

/****************************************************************//**
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
 *******************************************************************/

/****************************************************************//**
 * Publisher constructor.  Saves the URL, and starts a server
 * thread.
 *
 * @param component: The name of the publishing component.
 * @param transports: one or more of INPROC, IPC and TCP or'ed together.
 * @param portnum: The TCP port number. If not provided, will use a
 *                 transient port.
 * @param keymaster: If provided, the URL of the keymaster.
 *
 *******************************************************************/

Publisher::Publisher(std::string component, int transports,
                     int portnum, std::string keymaster)
    :
    _impl(new Publisher::PubImpl(component, transports, portnum, keymaster))

{
}

/****************************************************************//**
 * Signals the server thread that we're done, waiting for it to exit
 * on its own.
 *
 ********************************************************************/

Publisher::~Publisher()

{
    _impl.reset();
}

/****************************************************************//**
 * This template function serves to publish backend data via the
 * Publisher.  Since each server that published data has a different
 * data specification (unlike Samplers and Parameters), the Publisher
 * must be given data that is already serialized and ready to send.
 * The caller would thus fill out a GPB, serialize it to a
 * std::string, and call this function with that string.
 *
 * @param key: The publishing key for the data.
 * @param data: The serialized data, ready to send.
 *
 * @return true if the data was succesfuly placed in the publication
 * queue, false otherwise.
 *
 *******************************************************************/

bool Publisher::publish_data(std::string key, std::string data)
{
    return _impl->publish(key, data, 1);
}
