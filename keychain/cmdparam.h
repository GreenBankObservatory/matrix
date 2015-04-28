/*******************************************************************
 ** cmdparam.h - A command line parser class.  This class takes
 *                      in a string in the form:
 *
 *      CMD <d>param0<d>param1<d>...<d>paramn
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

#if !defined(_CMDPARAM_H_)
#define _CMDPARAM_H_

#include <string>
#include <vector>

class CmdParam

{
public:

    CmdParam(std::string delim = "\t ") : _delimiter(delim) {}

    ~CmdParam() {}


    void clear();
    size_t  count();
    bool  new_list(std::string);
    std::string cmd();
    std::string cmd_str();
    std::string comment();
    std::string operator[](int);

private:

    CmdParam(const CmdParam &);
    CmdParam &operator=(const CmdParam &);
    bool _parse_parameters(std::vector<std::string>);

    std::vector<std::string> _list;
    std::string _cmd_str;
    std::string _cmd;
    std::string _comment;
    std::string _delimiter;

    static std::string _def_buf;
};

/**
 * Clears the object.
 *
 */

inline void CmdParam::clear()

{
    _cmd = _cmd_str = _comment = _def_buf;
    _list.clear();


}

/**
 * Returns number of discreete parameters held in this object.
 *
 * @return An int, which holds the number of parameters held in this
 *         object.
 *
 */

inline size_t CmdParam::count()

{
    return _list.size();

}

/**
 * Returns the command portion of the command line
 *
 */

inline std::string CmdParam::cmd()

{
    return _cmd;

}

/**
 * Returns the original command line
 *
 */

inline std::string CmdParam::cmd_str()

{
    return _cmd_str;

}

/**
 * Returns the comment
 *
 */

inline std::string CmdParam::comment()

{
    return _comment;

}

#endif
