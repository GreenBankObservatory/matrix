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
#include <map>
#include <set>
#include <ostream>
#include <cstring>
#include <boost/algorithm/string.hpp>

struct timeval;
namespace matrix
{
    class MatrixException : public std::runtime_error
    {
    public:

        enum
        {
            MSGLEN = 300
        };

        MatrixException(std::string etype, std::string msg)
                : runtime_error(etype),
                  _msg(msg)
        {
        }

        virtual ~MatrixException() throw()
        {
        }


        virtual char const *what() const noexcept
        {
            std::ostringstream msg;
            msg << std::runtime_error::what() << ": " << _msg;
            memset((void *) _what, 0, MSGLEN + 1);
            strncpy((char *) _what, msg.str().c_str(), MSGLEN);
            return _what;
        }

    private:

        std::string _msg;
        char _what[MSGLEN + 1];
    };
};

namespace mxutils
{
    void do_nanosleep(int seconds, int nanoseconds);
    bool operator<(timeval &lhs, timeval &rhs);
    timeval operator+(timeval lhs, timeval rhs);
    timeval operator+(timeval lhs, double rhs);
    timeval operator-(timeval lhs, timeval rhs);
    std::ostream &operator<<(std::ostream &os, const timeval &t);
    std::string ToHex(const std::string &s, bool upper_case = false, size_t max_len = 0);

/**
 * This is a predicate function, intended to return true if a particular
 * char 'c' should be stripped out of the string. Here we are stripping
 * out non-numeric characters. The characters needed by the conversion
 * routines below are [0-9], [A-Fa-f], '.+-', (and 'e' for exponent,
 * already covered.) And 'x' in 0x hex designation.
 *
 * @param c: the character
 *
 * @return true if the character should be stripped out.
 *
 */

    inline bool is_non_numeric_p(char c)
    {
        static char cs[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
                            'A', 'B', 'C', 'D', 'E', 'F',
                            'a', 'b', 'c', 'd', 'e', 'f',
                            '.', '+', '-', 'x'};
        static std::set<char> syms(cs, cs + sizeof cs);
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

    inline std::string strip_non_numeric(const std::string &s)
    {
        std::string stripped = s;
        remove_if(stripped.begin(), stripped.end(), is_non_numeric_p);
        return stripped;
    }

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

        template<typename T>
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
        is_substring_in_p(std::string subs) : _subs(subs)
        {
        }

        bool operator()(std::string s)
        {
            return s.find(_subs) != std::string::npos;
        }

    private:
        std::string _subs;
    };

/**
 * Outputs a vector to an ostream
 *
 * @param v: the vector to be output
 *
 * @param o: the ostream type
 *
 */

    template<typename T>
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
 * Outputs a map to an ostream
 *
 * @param m: the map to be output
 *
 * @param o: the ostream type
 *
 */

    template <typename T, typename Q>
    void output_map(std::map<T,Q> m, std::ostream &o)
    {
        std::ostringstream str;
        str << "{";

        for (auto x: m)
        {
            str << x.first << ":" << x.second << ", ";
        }

        std::string s = str.str();
        std::string y(s.begin(), s.end() - 2);
        y += "}";
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
    template<typename T>
    T convert(const std::string &s)
    {
        return T(s);
    }

/* specializations */
    template<>
    inline std::string convert<std::string>(const std::string &s)
    {
        return s;
    }

    template<>
    inline int8_t convert<int8_t>(const std::string &s)
    {
        return (int8_t) stoi(strip_non_numeric(s), 0, 0);
    }

    template<>
    inline uint8_t convert<uint8_t>(const std::string &s)
    {
        return (uint8_t) stoul(strip_non_numeric(s), 0, 0);
    }

    template<>
    inline int16_t convert<int16_t>(const std::string &s)
    {
        return (int16_t) stoi(strip_non_numeric(s), 0, 0);
    }

    template<>
    inline uint16_t convert<uint16_t>(const std::string &s)
    {
        return (uint16_t) stoul(strip_non_numeric(s), 0, 0);
    }

    template<>
    inline int32_t convert<int32_t>(const std::string &s)
    {
        return (int32_t) stoi(strip_non_numeric(s), 0, 0);
    }

    template<>
    inline uint32_t convert<uint32_t>(const std::string &s)
    {
        return (uint32_t) stoul(strip_non_numeric(s), 0, 0);
    }

    template<>
    inline int64_t convert<int64_t>(const std::string &s)
    {
        return (int64_t) stol(strip_non_numeric(s), 0, 0);
    }

    template<>
    inline uint64_t convert<uint64_t>(const std::string &s)
    {
        return (uint64_t) stoul(strip_non_numeric(s), 0, 0);
    }

    template<>
    inline bool convert<bool>(const std::string &s)
    {
        bool v = false;

        if (s == "True" or s == "true")
        {
            v = true;
        }

        return v;
    }

    template<>
    inline double convert<double>(const std::string &s)
    {
        // [-+]?[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?
        return stod(strip_non_numeric(s));
    }

    template<>
    inline float convert<float>(const std::string &s)
    {
        // [-+]?[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?
        return stof(strip_non_numeric(s));
    }

    // Some iostream extractors, handy for debugging.

    template <typename T>
    std::ostream & operator << (std::ostream &o, std::vector<T> const &v)
    {
        if (v.empty())
        {
            o << "[]";
        }
        else
        {
            std::ostringstream str;
            str << "[";
            std::copy(v.begin(), v.end(), std::ostream_iterator<T>(str, ", "));
            std::string s = str.str();
            std::string y(s.begin(), s.end() - 2);
            y += "]";
            o << y;
        }

        return o;
    }

    template <typename T>
    std::ostream & operator << (std::ostream &o, std::list<T> const &v)
    {
        if (v.empty())
        {
            o << "()";
        }
        else
        {
            std::ostringstream str;
            str << "(";
            std::copy(v.begin(), v.end(), std::ostream_iterator<T>(str, ", "));
            std::string s = str.str();
            std::string y(s.begin(), s.end() - 2);
            y += ")";
            o << y;
        }

        return o;
    }

/**
 * Outputs a map to an ostream
 *
 * @param m: the map to be output
 *
 * @param o: the ostream type
 *
 */

    template <typename T, typename Q>
    std::ostream & operator << (std::ostream &o, std::map<T,Q> const &m)
    {
        if (m.empty())
        {
            o << "{}";
        }
        else
        {
            std::ostringstream str;
            str << "{";

            for (auto x: m)
            {
                str << x.first << " : " << x.second << ", ";
            }

            std::string s = str.str();
            std::string y(s.begin(), s.end() - 2);
            y += "}";
            o << y;
        }

        return o;
    }
};


#endif // _MATRIX_UTIL_H_
