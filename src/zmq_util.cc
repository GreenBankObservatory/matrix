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

using namespace std;

/********************************************************************
 * z_send(zmq::socket_t &sock, std::string data, int flags = 0)
 *
 * A simple utility to send stat from within a std::string over a 0MQ
 * connection.  The data could be a binary buffer, or ASCII strings.
 * 0MQ treats them the same.  The null terminator on a string will be
 * ignored.
 *
 * @param zmq::socket_t &sock: the socket to send over
 * @param zmq::std::string data: the data buffer to send
 * @param int flags: Socket options, such as ZMQ_SNDMORE.
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

/********************************************************************
 * z_recv(zmq::socket_t &sock, std::string &data)
 *
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
 * @param zmq::socket_t &sock: The socket that will be receiving the
 *        string.
 * @param std::string &data: the received string.
 *
 * @return A proper C++ string.
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

/********************************************************************
 * z_send(zmq::socket_t &sock, char const *buf, size_t sze, int flags)
 *
 * A simple utility to send stat from within a char buffer over a 0MQ
 * connection.  The data could be a binary, or ASCII strings.  0MQ
 * treats them the same.  The null terminator on a string will be
 * ignored.  If the buffer is *NOT* ASCII, 'sze' must be > 0.
 *
 * @param zmq::socket_t &sock: the socket to send over
 * @param const char *buf: the data buffer to send
 * @param size_t sze: the (optional) size of the buffer.  Not needed
 *        if the buffer contains ASCII data.
 * @param int flags: Socket options, such as ZMQ_SNDMORE.
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

/********************************************************************
 * z_recv(zmq::socket_t &sock, char *buf, size_t &sze)
 *
 * Simple utility to receive data into a C++ traditional buffer from a
 * 0MQ socket.  0MQ sends strings by encoding the size, followed by
 * the string, no null terminator.  It is not desirable to send the
 * string with a terminator, because some languages don't need it and
 * could be confused by it. Thus, in C++, where a terminator is
 * expected, one must be provided.
 *
 * @param zmq::socket_t &sock: The socket that will be receiving the
 *        data.
 * @param char *buf: the received data.
 * @param size_t &sze: The size of the given buffer, and upon return,
 *        the size of the received data.  If the size of the buffer is
 *        smaller than the received data, the difference will be lost.
 *
 *******************************************************************/

void z_recv(zmq::socket_t &sock, char *buf, size_t &sze)
{
    zmq::message_t msg;
    size_t size;
    sock.recv(&msg);
    size = std::min(sze, msg.size());
    memcpy(buf, msg.data(), size);
    sze = size;

    // If there is room, terminate the buffer with a null.  This will
    // do no harm if the data is binary, since the data buffer
    // contains all the data and there is room for the superfluous
    // null, but is needed if the data is ASCII.
    if (size < sze)
    {
        buf[size + 1] = 0;
    }
}
