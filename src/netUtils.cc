/*******************************************************************
 *  netUtils.cc - Implements network related utility functions.  Only
 *  for Linux, as it uses the STL.
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

#include "netUtils.h"

#include <netdb.h>
#include <iostream>
#include <errno.h>

#include <list>
#include <algorithm>
#include <iostream>

using namespace std;

/********************************************************************
 * is_canonical(string n)
 *
 * This predicate function determines whether a string is a canonical
 * hostname or not.  This currently is determined if the string passed
 * in has at least one period in it (host.domain), and that it is not
 * (or does not contain) 'localhost'.  Any other conditions may be
 * placed in here.
 *
 * @param string n: the string to examine.
 *
 * @return true if it meets the conditions, false otherwise.
 *
 *******************************************************************/

bool is_canonical(string n)
{
    if (n.find('.') == string::npos)
    {
	return false;
    }

    if (n.find("localhost") != string::npos)
    {
	return false;
    }

    return true;
}

/********************************************************************
 * getCanonicalHostname(string &name)
 *
 * Returns the local host's canonical host name
 * (e.g. 'ajax.gb.nrao.edu').  The function gathers all names by first
 * looking up the host name (by gethostname) then uses gethostbyname
 * to get all the full names and aliases, putting them all into a
 * list.  It then searches the list for the first element that meets
 * the conditions of a canonical host name (see the predicate
 * 'is_canonical' above.)
 *
 * @param string &name: A string buffer passed into the caller that
 *        will hold the canonical host name, if the return value is
 *        true.
 *
 * @return true on success, false otherwise.
 *
 *******************************************************************/

bool getCanonicalHostname(string &name)

{
    list<string> names;
    list<string>::iterator it;
    string buf(256, 0);
    struct hostent *hent;

    if (gethostname((char *)buf.data(), buf.size()) == 0)
    {
	buf.resize(buf.find('\0')); // accurately reflect string length
	names.push_back(buf);

	if ((hent = gethostbyname(buf.c_str())) != NULL)
	{
	    names.push_back(hent->h_name);

	    for (int i = 0; hent->h_aliases[i]; i++)
	    {
		names.push_back(hent->h_aliases[i]);
	    }

	    it = find_if(names.begin(), names.end(), is_canonical);

	    if (it != names.end())
	    {
		name = *it;
		return true;
	    }
	}
    }

    return false;
}
