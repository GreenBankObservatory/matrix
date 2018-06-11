/*******************************************************************
 *  zmq_util.cc - Implementation of the non-template utilities that
 *  handle ZeroMQ sockets.
 *
 *  Copyright (C) 2012 Associated Universities, Inc. Washington DC, USA.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hopey that it will be useful, but
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

#include "matrix/zmq_util.h"
#include "matrix/matrix_util.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <sstream>
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include <stdlib.h>

#if ZMQ_VERSION_MAJOR < 4
#warning "Using an older version of zero-mq. Consider upgrading"
#endif
using namespace std;
using namespace std::placeholders;
using namespace matrix;

namespace mxutils
{

// receives using a time-out. Useful to prevent clients from
// blocking forever.
    void z_recv_with_timeout(zmq::socket_t &sock, zmq::message_t &msg, int to);

// Sends using a time out.
    void z_send_with_timeout(zmq::socket_t &sock, zmq::message_t &msg, int flags, int to);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcomment"
/**
 * Generates a string of length 'len' with random alpha-numeric
 * characters in it. This is useful to generate unique inproc and ipc
 * urls. For example, if a class Foo internally uses an inproc pipe to
 * control a private thread, instantiating more than one instance of Foo
 * would require those URLs to be unique to each object. Otherwise a
 * zmq::error_t of "address already in use" would be thrown. A randomly
 * generated string would serve the purpose:
 *
 *     string pipe_url = string("inproc://") + gen_random_string(20);
 *     // pipe_url == inproc://S22JjzhMaiRrV41mtzxl
 *
 * @param len: the length of the string
 *
 * @return The string of random alpha-numeric characters.
 *
 */
#pragma GCC diagnostic pop

std::string gen_random_string(const int len)
{
    std::string s;
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    static bool initialized = false;

    if (!initialized)
    {
        srandom(time(NULL));
        initialized = true;
    }

    for (int i = 0; i < len; ++i)
    {
        s.push_back(alphanum[random() % (sizeof(alphanum) - 1)]);
    }

    return s;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcomment"
/**
 * Given a possibly incomplete urn, creates a completed urn.
 *
 * The rules are:
 *
 *   1 If only the transport is given, a transient URN will be
 *   generated:
 *     * tcp: tcp://\*:XXXXX (The XXXXXs indicate that the port should be
 *       randomly chosen from the ephemeral range during bind.)
 *     * ipc: ipc:///tmp/<20 randomly chosen alphanumeric characters>
 *     * inproc: inproc://<20 randomly chosen alphanumeric characters.
 *
 *   2 A valid partial non-tcp urn may be given. A partial URN is a URN
 *   which ends in a string of 'X's: ipc:///tmp/XXXXXX. In this case,
 *   the Xs will be replaced by random alphanumeric characters, one for
 *   each X.
 *
 *   3 A valid partial tcp urn. This is a URN that does not end in
 *   ':12345', where '12345' may be any 5-digit value < 32768; for
 *   example, 'tcp://\*', or 'tcp://ajax.gb.nrao.edu'. A ':XXXXX' will be
 *   appended to the URN to indicate that a port from the ephemeral
 *   range should be chosen.
 *
 *   4 A valid complete urn: No trailing XXXXs for the non-tcp
 *   transports, or a tcp URN complete with valid port number: returned
 *   as-is.
 *
 *   5 None of the above: the URN is returned empty.
 *
 * example:
 *
 *     string urn = process_zmq_urn("inproc");
 *     // urn now "inproc://S22JjzhMaiRrV41mtzxl
 *     string urn2 = process_zmq_urn("ipc://foobar_XXXXXXXXXX");
 *     // urn2 now "ipc://foobar_aiRrV41mtzx
 *     string urn3 = process_zmq_urn("tcp");
 *     // urn3 now "tcp://\*:XXXXX
 *     string urn4 = process_zmq_urn("tcp://\*:4242");
 *     // urn4 == input urn.
 *
 * @param urn: the starting URN. It may be a transport, or an incomplete
 * urn in which case it will be completed. Incomplete urns are those
 * which end in some number of 'X's, such as 'XXXXX'
 *
 * @return A complete URN.
 *
 */
#pragma GCC diagnostic pop

std::string process_zmq_urn(const std::string input)
{
    string s(input);
    boost::regex p_tcp("^tcp"), p_inproc("^inproc"), p_ipc("^ipc");
    boost::regex p_port(":[0-9]{1,5}$"), p_bad_port(":[0-9][A-Za-z]*"), p_xs("X+$");
    boost::smatch result;

    if (boost::regex_search(s, result, p_inproc) || boost::regex_search(s, result, p_ipc))
    {
        // Transport is there; but if it matches, it's the only
        // thing. Make a temp URN out of it.
        if (boost::regex_match(s, result, p_inproc) || boost::regex_match(s, result, p_ipc))
        {
            // just the transport given. Tack on '://' + 20 random
            // alphanumeric characters. If 'ipc', add '/tmp', so that
            // the pipe gets put in there.
            if (s.find("ipc") != string::npos)
            {
                return s + ":///tmp/" + gen_random_string(20);
            }

            return s + "://" + gen_random_string(20);
        }

        // It doesn't match just the transport. Look for a partial or
        // complete URN:
        if (s.find("://") != string::npos)
        {

            // is urn in form 'ipc://foo.bar.XXXX'? replace all
            // trailing Xs with an equivalent number of randomly
            // generated alphanumeric characters
            if (boost::regex_search(s, result, p_xs))
            {
                string match = result[0];
                string replacement = gen_random_string(match.size());
                return boost::regex_replace(s, p_xs, replacement);
            }

            // otherwise just return as given.
            return s;
        }
    }

    if (boost::regex_search(s, result, p_tcp))
    {
        auto fn_equal_to = bind(equal_to<char>(), _1, ':');

        if (count_if(s.begin(), s.end(), fn_equal_to) <= 2)
        {
            if (boost::regex_match(s, result, p_tcp))
            {
                // Found just the transport
                return s + "://*:XXXXX";
            }

            // look for a partial or complete URN:
            if (s.find("://") != string::npos)
            {
                // check to see if it has a port.
                if (!boost::regex_search(s, result, p_port))
                {
                    if (boost::regex_search(s, result, p_bad_port))
                    {
                        // found some weird ':xxx' stuff
                        return string();
                    }

                    return s + ":XXXXX";
                }

                // found a port, return as is.
                return s;
            }
        }

    }

    return string(); // Nope.
}

/**
 * A simple utility to send stat from within a std::string over a 0MQ
 * connection.  The data could be a binary buffer, or ASCII strings.
 * 0MQ treats them the same.  The null terminator on a string will be
 * ignored.
 *
 * @param sock: the socket to send over
 * @param data: the data buffer to send
 * @param flags: Socket options, such as ZMQ_SNDMORE.
 *
 */

    void z_send(zmq::socket_t &sock, std::string data, int flags, int to)
    {
        zmq::message_t msg(data.size());
        memcpy((char *)msg.data(), data.data(), data.size());

        if (to)
        {
            z_send_with_timeout(sock, msg, flags, to);
        }
        else
        {
            sock.send(msg, flags);
        }
    }

/**
 * Simple utility to receive C++ string types over a 0MQ socket.  0MQ
 * sends strings by encoding the size, followed by the string, no null
 * terminator.  It is not desirable to send the string with a
 * terminator, because some languages don't need it and could be
 * confused by it. Thus, in C++, where a terminator is expected, one
 * must be provided.
 *
 * @param sock: The socket that will be receiving the string.
 *
 * @param data: the received string.
 *
 * @param to: A time-out value, in milliseconds. If not 0, it will be
 * used. If 0, then a traditional 0MQ recv() is performed, which
 * blocks indefinitely.
 *
 */

    void z_recv(zmq::socket_t &sock, std::string &data, int to)
    {
        zmq::message_t msg;

        if (to)
        {
             z_recv_with_timeout(sock, msg, to);
        }
        else
        {
            sock.recv(&msg);
        }

        data.clear();
        data.resize(msg.size() + 1, 0);
        memcpy((char *)data.data(), msg.data(), msg.size());
        data.resize(msg.size());
    }

/**
 * A simple utility to send data from within a char buffer over a 0MQ
 * connection.  The data could be a binary, or ASCII strings.  0MQ
 * treats them the same.  The null terminator on a string will be
 * ignored.  If the buffer is *NOT* ASCII, 'sze' must be > 0.
 *
 * @param sock: the socket to send over
 * @param buf: the data buffer to send
 * @param sze: the (optional) size of the buffer.  Not needed
 *        if the buffer contains ASCII data.
 * @param flags: Socket options, such as ZMQ_SNDMORE.
 *
 */

    void z_send(zmq::socket_t &sock, const char *buf, size_t sze, int flags, int to)
    {
        if (!sze)
        {
            sze = strlen(buf);
        }

        zmq::message_t msg(sze);
        memcpy((char *)msg.data(), buf, sze);

        if (to)
        {
            z_send_with_timeout(sock, msg, flags, to);
        }
        else
        {
            sock.send(msg, flags);
        }
    }

/**
 * Simple utility to receive data into a C++ traditional buffer from a
 * 0MQ socket.  0MQ sends strings by encoding the size, followed by
 * the string, no null terminator.  It is not desirable to send the
 * string with a terminator, because some languages don't need it and
 * could be confused by it. Thus, in C++, where a terminator is
 * expected, one must be provided.
 *
 * @param sock: The socket that will be receiving the data.
 *
 * @param buf: the received data.
 *
 * @param sze: The size of the given buffer, and upon return, the size
 *        of the received data.  If the size of the buffer is smaller
 *        than the received data, the difference will be lost.
 *
 * @param to: A time-out value, in milliseconds. If not 0, it will be
 * used. If 0, then a traditional 0MQ recv() is performed, which
 * blocks indefinitely.
 *
 */

    void z_recv(zmq::socket_t &sock, char *buf, size_t &sze, int to)
    {
        zmq::message_t msg;
        size_t size;

        if (to)
        {
            z_recv_with_timeout(sock, msg, to);
        }
        else
        {
            sock.recv(&msg);
        }

        size = std::min(sze, msg.size());
        memset(buf, 0, sze);
        memcpy(buf, msg.data(), size);
        sze = size;
    }

/**
 * Receives a multipart message all in one call, storing each frame of
 * the message as a raw string buffer in the provided vector of
 * strings. This should be called after a poll indicates that the socket
 * is ready to read. Used this way the function will return 0 or more
 * frames without hanging.
 *
 * @param sock: the socket to read.
 *
 * @param data: the vector of strings where the frames are stored. Each
 * element is a string representing the raw bytes of the frame.
 *
 */

    void z_recv_multipart(zmq::socket_t &sock, std::vector<std::string> &data)
    {
        int more;
        size_t more_size = sizeof(more);

        sock.getsockopt(ZMQ_RCVMORE, &more, &more_size);
        data.clear();

        while (more)
        {
            string frame;
            z_recv(sock, frame);
            data.push_back(frame);
            sock.getsockopt(ZMQ_RCVMORE, &more, &more_size);
        }
    }

    static bool _get_min_max_ephems(int &min, int &max);

/**
 * Tries to bind a ZMQ socked to an ephemeral address. The function
 * first checks the ZeroMQ version; in versions 3.2 and above this is
 * directly supported. If the version is less than 3.2, an attempt is
 * made to obtain an ephemeral port by looking at the range of ephemeral
 * ports, choosing one at random, and binding to it. If the bind fails,
 * this process is repeated 'retries' times.
 *
 * @param s: The ZeroMQ socket.
 * @param t: The URL.
 * @param retries: the number of times this function should try to
 * bind to an ephemeral port before declaring failure.
 *
 * @return An int, the port number bound, or an error:
 *         -1: Could not open the ephem proc file
 *         -2: Could not bind in 'retries' retries.
 *         -3: Malformed URL in was passed in in 't'.
 *
 */

    int zmq_ephemeral_bind(zmq::socket_t &s, std::string t, int retries)
    {
        int i, k, min, max, range;
        int major, minor, patch;

        zmq_version(&major, &minor, &patch);

        if (major >= 3 && minor >= 2)
        {
            try
            {
                s.bind(t.c_str());
                std::string port(1024, 0);
                size_t size = port.size();

                s.getsockopt(ZMQ_LAST_ENDPOINT, (void *)port.data(), &size);
                std::vector<std::string> components;
                boost::split(components, port, boost::is_any_of(":"));
                // port number will be the last element
                i = atoi(components.back().c_str());
                return i;
            }
            catch (zmq::error_t &e)
            {
                // Didn't work; drop into manual way below.
            }
        }

        // Here because version < 3.2, or the above failed, try the manual way.
        if (!_get_min_max_ephems(min, max))
        {
            return -1; // couldn't open the proc file
        }

        range = max - min;
        // URL passed in in 't' is '<transport>://*:*, for example
        // 'tcp://*:*'. This is to accomodate later versions of ZMQ. For
        // earlier versions, where we must get the ephem port ourselves, we
        // need just the 'tcp://*' part. Split on ":" and leave out the last
        // component (if it exists; the first two should be there or it is a
        // malformed URL).
        std::vector<std::string> components;
        std::string base_url;
        boost::split(components, t, boost::is_any_of(":"));

        if (components.size() >= 2)
        {
            base_url = components[0] + ":" + components[1];
        }
        else
        {
            return -3; // malformed URL
        }

        if (retries)
        {
            // Grab a number in the ephemeral range, construct the URL,
            // and try. Keep trying for 'retries', or until success.

            for (k = 0; k < retries; ++k)
            {
                std::ostringstream url;
                i = min + (rand() % range + 1);
                url << base_url << ":" << i;

                try
                {
                    s.bind(url.str().c_str());
                }
                catch (zmq::error_t &e)
                {
                    continue;
                }

                return i; // i bound, if we got this far.
            }
        }

        return -2; // looped through the entire range without success.
    }

    bool _get_min_max_ephems(int &min, int &max)
    {
        FILE *feph = NULL;
        std::string fname("/proc/sys/net/ipv4/ip_local_port_range");

        feph = fopen(fname.c_str(), "r");

        if (feph == NULL)
        {
            perror("zmq_ephemeral_bind()");
            return false;
        }

        fscanf(feph, "%i %i", &min, &max);
        fclose(feph);
        return true;
    }

    void z_recv_with_timeout(zmq::socket_t &sock, zmq::message_t &msg, int to)
    {
        // Poll socket for a reply, with timeout
#if ZMQ_VERSION_MAJOR > 3
        zmq::pollitem_t items[] = { { (void *)sock, 0, ZMQ_POLLIN, 0 } };
#else
        zmq::pollitem_t items[] = { { sock, 0, ZMQ_POLLIN, 0 } };
#endif
        zmq::poll (&items[0], 1, to);

        // If we got a reply, process it
        if (items[0].revents & ZMQ_POLLIN)
        {
            // We got a reply from the server, all is well!
            sock.recv(&msg);
        }
        else
        {
            throw MatrixException("z_recv_with_timeout",
                                  "receive timed out without a response");
        }
    }

    void z_send_with_timeout(zmq::socket_t &sock, zmq::message_t &msg, int flags, int to)
    {
#if ZMQ_VERSION_MAJOR > 3
        zmq::pollitem_t items[] = {{(void *)sock, 0, ZMQ_POLLOUT, 0}};
#else
        zmq::pollitem_t items[] = {{sock, 0, ZMQ_POLLOUT, 0}};
#endif
        zmq::poll(&items[0], 1, to);

        if (items[0].revents & ZMQ_POLLOUT)
        {
            // we can send at least 1 byte
            sock.send(msg, flags);
        }
        else
        {
            throw MatrixException("z_send_with_timeout",
                                  "send timed out.");
        }
    }
}
