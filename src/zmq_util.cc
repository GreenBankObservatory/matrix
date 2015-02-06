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

}
