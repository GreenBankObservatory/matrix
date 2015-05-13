/*******************************************************************
 *  matrix_util.h - Useful odds & ends
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

#if !defined _MATRIX_UTIL_H_
#define _MATRIX_UTIL_H_

#include <string>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <iterator>
#include <vector>

#include <boost/algorithm/string.hpp>

class MatrixException : public std::runtime_error
{
public:

    MatrixException(std::string etype, std::string msg)
        : runtime_error(etype),
          _msg(msg)
    {
    }

    virtual ~MatrixException() throw ()
    {
    }


    virtual const char* what() const throw()
    {
        std::ostringstream msg;
        msg << std::runtime_error::what() << ": " << _msg;
        return msg.str().c_str();
    }

private:

    std::string _msg;
};


namespace mxutils
{
/**
 * \class fn_string_join is a simple functor that provides a handy way to
 * join strings from a container of strings, using the delimiter
 * provided. It may be used stand-alone, but is most useful with
 * high-level functions like transform, etc.
 *
 */

    struct fn_string_join
    {
        fn_string_join(std::string delim)
        {
            _delim = delim;
        }

        template <typename T>
        std::string operator()(T x)
        {
            return boost::algorithm::join(x, _delim);
        }

    private:

        std::string _delim;
    };

/**
 * \class is_substring_in_p
 *
 * A predicate functor which checks a given string for a provided
 * substring and returns true if it exists, false otherwise. The
 * substring to find is provided in the constructor:
 *
 *      is_substring_in_p ssp("fox");
 *      ssp("the quick brown dog jumped over the lazy fox"); // returns true
 *
 */

    struct is_substring_in_p
    {
        is_substring_in_p(std::string subs) : _subs(subs) {}

        bool operator ()(std::string s)
        {
            return s.find(_subs) != std::string::npos;
        }

    private:
        std::string _subs;
    };

    bool is_numeric_p(char c);
    std::string strip_non_numeric(const std::string &s);

/**
 * Outputs a vector to an ostream
 *
 * @param v: the vector to be output
 *
 * @param o: the ostream type
 *
 */

    template <typename T>
    void output_vector(std::vector<T> v, std::ostream &o)
    {
        std::ostringstream str;
        str << "[";
        std::copy(v.begin(), v.end(), std::ostream_iterator<T>(str, ", "));
        std::string s = str.str();
        std::string y(s.begin(), s.end() - 2);
        y += "]";
        o << y;
    }

/**
 * These template specializations convert 's' to a value of type T and
 * return it.
 *
 * Add new ones as needed.
 *
 * @param const string &s: The std::string representation of the value
 *
 * @return The T.
 *
 */

/* General template */
    template <typename T> T convert(const std::string &s)
    {
        return T(s);
    }

/* specializations */
    template <> inline std::string convert<std::string>(const std::string &s)
    {
        return s;
    }

    template <> inline int convert<int>(const std::string &s)
    {
        return stoi(strip_non_numeric(s));
    }

    template <> inline unsigned int convert<unsigned int>(const std::string &s)
    {
        return (unsigned int)stoul(strip_non_numeric(s));
    }

    template <> inline double convert<double>(const std::string &s)
    {
        // [-+]?[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?
        return stod(strip_non_numeric(s));
    }

    template <> inline float convert<float>(const std::string &s)
    {
        // [-+]?[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?
        return stof(strip_non_numeric(s));
    }

}


#endif // _MATRIX_UTIL_H_
