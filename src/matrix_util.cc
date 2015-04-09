/*******************************************************************
 *  matrix_util.cc - some useful utilities
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

#include "matrix_util.h"
#include <set>
#include <string>
#include <algorithm>

using namespace std;

namespace mxutils
{

/**
 * This is a predicate function, intended to return true if a particular
 * char 'c' should be stripped out of the string. Here we are stripping
 * out non-numeric characters. The characters needed by the conversion
 * routines below are [0-9], [A-Fa-f], '.+-', (and 'e' for exponent,
 * already covered. So far no need for 'x' in 0x hex designation).
 *
 * @param c: the character
 *
 * @return true if the character should be stripped out.
 *
 */


    bool is_numeric_p(char c)
    {
        static char cs[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
                            'A', 'B', 'C', 'D', 'E', 'F',
                            'a', 'b', 'c', 'd', 'e', 'f',
                            '.', '+', '-'};
        static set<char> syms(cs, cs + sizeof cs);
        return syms.find(c) == syms.end();
    }

/**
 * This helper will strip out whatever characters the predicate
 * 'strip_p' returns 'true' for (see above).
 *
 * @param s: the string to be stripped
 *
 * @return a new string, which is 's' minus the non-numeric characters.
 *
 */

    string strip_non_numeric(const string &s)
    {
        string stripped = s;
        stripped.erase(remove_if(stripped.begin(), stripped.end(), is_numeric_p), stripped.end());
        return stripped;
    }
}
