/*******************************************************************
 *  zmq_util.h -- Some handy utilities to handle ZeroMQ sockets.
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

#if !defined _ZMQ_UTIL_H_
#define _ZMQ_UTIL_H_

#include "zmq.hpp"

#include <string>
#include <vector>

namespace mxutils
{

    // Send and receive for std::strings.

    void z_send(zmq::socket_t &sock, std::string const data, int flags, int to = 0);
    void z_recv(zmq::socket_t &sock, std::string &data, int to = 0);

    // and for traditional strings/buffers.  (Set 'sze' to 0 if 'buf' is ASCII)
    void z_send(zmq::socket_t &sock, const char *buf, size_t sze, int flags, int to = 0);
    void z_recv(zmq::socket_t &sock, char *buf, size_t &sze, int to = 0);

    // receives multiple frames in one call.
    void z_recv_multipart(zmq::socket_t &sock, std::vector<std::string> &data);

    /**
     * A send function for simple data types.  These data types must be
     * contiguous in memory: ints, doubles, bools, and structs of these
     * types, etc.
     *
     * @param sock: the socket to send the data over
     * @param data: the data to send
     * @param flags: socket flags, such as ZMQ_SNDMORE
     *
     */

    template <class T> void z_send(zmq::socket_t &sock, const T &data, int flags, int to = 0)

    {
        size_t sze = sizeof data;
        z_send(sock, (const char *)&data, sze, flags, to);
    }

    /**
     * A receive function template for simple data types.  These data types must
     * meet the same requirements for the basic z_send(), i.e. be
     * contiguous in memory.
     *
     * @param sock: the socket to receive from
     *
     * @param data: the data received.
     *
     * @param to: A time-out value, in milliseconds. If not 0, it will be
     * used. If 0, then a traditional 0MQ recv() is performed, which
     * blocks indefinitely.
     *
     */

    template <class T> void z_recv(zmq::socket_t &sock, T &data, int to = 0)
    {
        size_t sze = sizeof data;
        z_recv(sock, (char *)&data, sze, to);
    }

    // bind a socket to an ephemeral port
    int zmq_ephemeral_bind(zmq::socket_t &s, std::string t, int retries = 10);

    // generate a random string, use with inproc and ipc urls
    std::string gen_random_string(const int len);

    // process urns for zmq services
    std::string process_zmq_urn(const std::string input);

}

#endif // _ZMQ_UTIL_H_
