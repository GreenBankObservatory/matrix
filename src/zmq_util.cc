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

#include "zmq_util.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <boost/algorithm/string.hpp>

using namespace std;

namespace mxutils
{

/****************************************************************//**
 * A simple utility to send stat from within a std::string over a 0MQ
 * connection.  The data could be a binary buffer, or ASCII strings.
 * 0MQ treats them the same.  The null terminator on a string will be
 * ignored.
 *
 * @param sock: the socket to send over
 * @param data: the data buffer to send
 * @param flags: Socket options, such as ZMQ_SNDMORE.
 *
 *******************************************************************/

void z_send(zmq::socket_t &sock, std::string data, int flags)
{
    zmq::message_t msg(data.size());
    memcpy((char *)msg.data(), data.data(), data.size());

    if (flags)
    {
        sock.send(msg, flags);
    }
    else
    {
        sock.send(msg);
    }
}

/****************************************************************//**
 * Simple utility to receive C++ string types over a 0MQ socket.  0MQ
 * sends strings by encoding the size, followed by the string, no null
 * terminator.  It is not desirable to send the string with a
 * terminator, because some languages don't need it and could be
 * confused by it. Thus, in C++, where a terminator is expected, one
 * must be provided.
 *
 * NOTE: This should ONLY be used if what is expected is an ASCII
 * string.
 *
 * @param sock: The socket that will be receiving the string.
 *
 * @param data: the received string.
 *
 *******************************************************************/

void z_recv(zmq::socket_t &sock, std::string &data)
{
    zmq::message_t msg;
    sock.recv(&msg);
    data.clear();
    data.resize(msg.size() + 1, 0);
    memcpy((char *)data.data(), msg.data(), msg.size());
    data.resize(msg.size());
}

/****************************************************************//**
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
 *******************************************************************/

void z_send(zmq::socket_t &sock, const char *buf, size_t sze, int flags)
{
    if (!sze)
    {
        sze = strlen(buf);
    }

    zmq::message_t msg(sze);
    memcpy((char *)msg.data(), buf, sze);

    if (flags)
    {
        sock.send(msg, flags);
    }
    else
    {
        sock.send(msg);
    }
}

/****************************************************************//**
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
 *******************************************************************/

void z_recv(zmq::socket_t &sock, char *buf, size_t &sze)
{
    zmq::message_t msg;
    size_t size;
    sock.recv(&msg);
    size = std::min(sze, msg.size());
    memset(buf, 0, sze);
    memcpy(buf, msg.data(), size);
    sze = size;
}

static bool _get_min_max_ephems(int &min, int &max);

/****************************************************************//**
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
 *******************************************************************/

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
        catch (zmq::error_t e)
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
            catch (zmq::error_t e)
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

}
