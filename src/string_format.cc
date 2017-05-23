/*
Copyright (C) 2017 Associated Universities, Inc. Washington DC, USA.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

Correspondence concerning GBT software should be addressed as follows:
      GBT Operations
      Green Bank Observatory
      P. O. Box 2
      Green Bank, WV 24944-0002 USA
*/
#include <string>
#include <cstring>
#include "matrix/string_format.h"

/// string_format - a template to format args into a std::string using
/// a modern C++ varargs template. The routine guards against overflow
/// using the snprintf (POSIX) 'what size do I need' trick.
/// The function returns a std::string with its contents formatted as
/// if cstrings were used.

namespace matrix
{
    /// Something things that should be in the STL ...
    bool str_strcasecmp(const std::string a, const std::string b)
    {
        return strcasecmp(a.c_str(), b.c_str()) == 0;
    }

    bool str_strncasecmp(const std::string a, const std::string b, const int nchar)
    {
        return strncasecmp(a.c_str(), b.c_str(), nchar) == 0;
    }

    size_t substring_in(const std::string needle, const std::string haystack, 
                        bool case_insensitive)
    {
        const char *p;
        if (case_insensitive)
        {
            p = strcasestr(haystack.c_str(), needle.c_str());
        }
        else
        {
            p = strstr(haystack.c_str(), needle.c_str());
        }
        if (p==nullptr)
        {
            return std::string::npos;
        }
        else
        {
            // return the position of the start of the match
            return p-haystack.c_str();
        }
    }
};

