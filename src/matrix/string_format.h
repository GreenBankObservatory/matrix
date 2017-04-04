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
#ifndef string_printf_h
#define string_printf_h

/// string_format - a template to format args into a std::string using
/// a modern C++ varargs template. The routine guards against overflow
/// using the snprintf (POSIX) 'what size do I need' trick.
/// The function returns a std::string with its contents formatted as
/// if cstrings were used.

namespace matrix
{
    template<typename ... Args>
    string string_format( const std::string& format, Args ... args )
    {
        // query the length we need by providing a zero sized buffer, 
        // and adding extra space for the terminating \0
        size_t size = snprintf( nullptr, 0, format.c_str(), args ... ) + 1; 
        unique_ptr<char[]> buf( new char[ size ] ); 
        snprintf( buf.get(), size, format.c_str(), args ... );
        return string( buf.get(), buf.get() + size - 1 ); 
    }
};

#endif


