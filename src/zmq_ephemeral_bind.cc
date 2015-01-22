/*******************************************************************
 *  zmq_ephemeral_bind.cc -- implements utility function
 *  'zmq_ephemeral_bind' which binds a ZMQ socket to a random port in
 *  the ephemeral region.
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

#include "zmq_ephemeral_bind.h"
#include "zmq.hpp"
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <sstream>

#include <stdio.h>
#include <errno.h>

#include <boost/algorithm/string.hpp>

static bool _get_min_max_ephems(int &min, int &max);

/********************************************************************
 * zmq_ephemeral_bind(zmq::socket_t &s, std::string t, int retries)
 *
 * Tries to bind a ZMQ socked to an ephemeral address. The function
 * first checks the ZeroMQ version; in versions 3.2 this is directly
 * supported. If the version is less than 3.2, an attempt is made to
 * obtain an ephemeral port by looking at the range of ephemeral ports,
 * choosing one at random, and binding to it. If the bind fails, this
 * process is repeated 'retries' times.
 *
 * @param zmq::socket &s: The ZeroMQ socket.
 * @param std::string t: The URL.
 * @param int retries: the number of times this function should try to
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
