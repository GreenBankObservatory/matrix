/*******************************************************************
 *  fixed_buffer.h - A fixed length buffer template, compatible with
 *  std::string. Intended for use with tsemfifo.
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

#if !defined(_FIXED_BUFFER_H_)
#define _FIXED_BUFFER_H_

#include <string>
#include <algorithm>
#include <string.h>
#include <iostream>

/**
 * \class fixed_buffer
 *
 * This class is intended to be used with tsemfifo in situations where
 * a fixed-length buffer is needed, and memory allocation must occur
 * ahead of time, not during the operation of the fifo. To accomplish
 * this we use a template based on an integer, which will be the
 * length of the buffer in bytes, and define operator= for both
 * fixed_buffer and string to ensure that when b = a, a's contents are
 * copied (using memcpy) into b's buffer. If 'a' is a std::string, the
 * copy operator will copy the min(a.size(), N) number of bytes, where
 * N is the size of the fixed buffer. This will not compile:
 *
 *      fixed_buf<100> bufA;
 *      fixed_buf<500> bufB;
 *
 *      bufA = bufB; // error, no operator = for these.
 *
 * Memory allocation occurs when the buffer is created, such as when a
 * tsemfifo of these is declared:
 *
 *      tsemfifo<fixed_buf<1000> > fifo(1000); // all memory allocated
 *      here.
 *
 * here a fifo of 1000 entries, each of 1000 bytes, is created.
 *
 */

template <size_t N>
struct fixed_buffer
{
    fixed_buffer() : _buf(N, 0) {}

    fixed_buffer & operator=(fixed_buffer const &rhs)
    {
        memcpy((char *)_buf.data(), rhs._buf.data(), N);
        return *this;
    }

    fixed_buffer & operator=(std::string const &rhs)
    {
        size_t sze = std::min(N, rhs.size());
        memcpy((char *)_buf.data(), rhs.data(), sze);
        return *this;
    }

    void set(int c)
    {
        memset((char *)_buf.data(), c, N);
    }

    char * data()
    {
        return (char *)_buf.data();
    }

private:
    std::string _buf;
};


struct flex_buffer
{
    flex_buffer() {}

    flex_buffer & operator=(flex_buffer const &rhs)
    {
        size_t sze = std::max(_buf.size(), rhs.size());
        _resize(sze);
        memcpy((char *)_buf.data(), rhs._buf.data(), sze);
        return *this;
    }

    flex_buffer & operator=(std::string const &rhs)
    {
        size_t sze = std::max(_buf.size(), rhs.size());
        _resize(sze);
        memcpy((char *)_buf.data(), rhs.data(), sze);
        return *this;
    }

    void set(int c)
    {
        _resize(1);
        memset((char *)_buf.data(), c, _buf.size());
    }

    char * data()
    {
        return (char *)_buf.data();
    }

    std::string &buffer()
    {
        return _buf;
    }

    size_t size() const
    {
        return _buf.size();
    }

private:

    void _resize(size_t size)
    {
        if (size > _buf.size())
        {
            _buf.resize(size);
        }
    }
    
    std::string _buf;
};

#endif
