/*******************************************************************
 ** cmdparam.cc - A simple command line parser class.  This class takes
 *                in a string in the form:
 *
 *      CMD <d>param0<d>param1<d>...<d>paramn [# comment]
 *
 *      where:
 *          CMD is a command name
 *          <d> is a delimiting character (set in constructor)
 *          param is a parameter; anything between two delimiters
 *          is considered the param, and if a param needs to contain
 *          a delimiter character, the entire param can be placed
 *          within double quotes.
 *
 * The CMD part can be obtained through the Cmd() member function; a
 * count of parameters can be had through the Count() menber function,
 * and the parameters through operator[], given the parameter index
 * (0 based).
 *
 *  Copyright (C) 1995, 2015 Associated Universities, Inc. Washington DC, USA.
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

#include "cmdparam.h"
#include "matrix/matrix_util.h"

#include <algorithm>
#include <boost/algorithm/string.hpp>

using namespace std;
using namespace mxutils;
using namespace std::placeholders;

string CmdParam::_def_buf("-8181");

/**
 * new_list() takes a string containing comma delimited values and breaks
 * them up into sub-strings, each of which will be pointed to by a <p
 * list[]> member.  new_list will also count and record the number of
 * parameters.
 *
 *  @param char * | prmstring | The string containing the comma delimited
 *        command parameters.
 *
 *  @return description
 *
 */

static string trim(string in)
{
    boost::trim(in);
    return in;
}

bool CmdParam::new_list(string cmdline)

{
    string cmdl;

    clear();
    // strip out spaces at the end & save original
    _cmd_str = cmdl = trim(cmdline);

    vector<string> parts;
    boost::split(parts, cmdl, boost::is_any_of(_delimiter), boost::token_compress_on);

    if (parts.empty())
    {
        return false;
    }

    _cmd = parts.front();
    vector<string> rest(parts.begin() + 1, parts.end());
    return _parse_parameters(rest);
}

/**
 * In new_list() the parameters were crudely split up using whitespaces
 * as tokens to split on. However this also splits up double-quote
 * enclosed single parameters, and text after the "#" character, which
 * denotes a comment. For instance, if new_list() receives the string
 *
 *     foo bar "Hello, World!" # store the string into bar
 *
 * then 'cmd' should be foo, and there should be two parameters, "bar"
 * and "Hello, World!", and a comment. "Hello, World!" needs to be
 * reassembled, as does the comment. It stores the corrected parameter
 * list in 'list', and the comment in 'comment'.
 *
 * @param rawlist: The preliminary split of the parameters, which should
 * be reassembled as explained above.
 *
 * @return true on success, false otherwise.
 *
 */

bool CmdParam::_parse_parameters(vector<string> rawlist)

{
    vector<vector<string> > pieces;
    vector<vector<string> >::iterator pi;
    vector<string>::iterator i;

    for (i = rawlist.begin(); i != rawlist.end(); ++i)
    {
        vector<string> bits;

        if (i->at(0) == '#')
        {
            // everything from here on is a comment. Join them up and
            // break;
            fn_string_join fn(" ");
            _comment = fn(vector<string>(i, rawlist.end()));
            break;
        }

        // opens a quoted string
        if (i->at(0) == '"')
        {
            // don't include the quote, it's just for this.
            bits.push_back(string(i->begin() + 1, i->end()));

            // continue adding to this param as long as back is not end
            // of quote
            while (i->at(i->size() - 1) != '"')
            {
                ++i;

                if (i->at(i->size() - 1) == '"')
                {
                    bits.push_back(string(i->begin(), i->end() - 1));
                }
                else
                {
                    bits.push_back(*i);
                }
            }
        }
        else
        {
            bits.push_back(*i);
        }

        pieces.push_back(bits);
    }

    _list.clear();
    _list.resize(pieces.size());
    transform(pieces.begin(), pieces.end(), _list.begin(), fn_string_join(" "));
    return true;
}

/**
 * Returns the specified parameter string.  If the index is out of range
 * (greater than the ordinal number of the last paramter) operator[]()
 * will return the default string.
 *
 *  @param specifies the _i_th parameter string.
 *
 *  @return The address of the specified parameter string.
 *
 */

string CmdParam::operator[](int i)

{
    if (i >= _list.size())
    {
        return _def_buf;
    }

    return _list[i];

}
