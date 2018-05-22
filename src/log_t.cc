/*******************************************************************
 *  log_t.cc - Logger class for VEGAS
 *
 *  Copyright (C) 2016 Associated Universities, Inc. Washington DC, USA.
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

#include "matrix/log_t.h"

// for stat
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <vector>
#include <tuple>

using namespace std;

namespace matrix
{
    int log_t::_log_level = INFO_LEVEL;
    std::list<std::shared_ptr<log_t::Backend>> log_t::backends;

    log_t::ostreamBackend::ostreamBackend(std::ostream &s)
        : os(s)
    {
        _level_name[DEBUG_LEVEL]   = "DEBUG";
        _level_name[INFO_LEVEL]    = "INFO";
        _level_name[WARNING_LEVEL] = "WARNING";
        _level_name[ERROR_LEVEL]   = "ERROR";
        _level_name[FATAL_LEVEL]   = "FATAL";

    }

    void log_t::ostreamBackend::output(log_t::Message &m)
    {
        stringstream s;

        s << _level_name[m.msg_level] << ":" << m.module << "--"
          << Time::isoDateTime(m.msg_time) << "--" << m.msg;

        os << s.str() << endl;
    }

    log_t::ostreamBackendColor::ostreamBackendColor(std::ostream &s)
        : ostreamBackend(s)
    {
       _level_color[DEBUG_LEVEL]   = LIGHT_CYAN;
       _level_color[INFO_LEVEL]    = LIGHT_GREEN;
       _level_color[WARNING_LEVEL] = MAGENTA;
       _level_color[ERROR_LEVEL]   = LIGHT_RED;
       _level_color[FATAL_LEVEL]   = RED;
    }

    void log_t::ostreamBackendColor::output(log_t::Message &m)
    {
        stringstream s;

        s << _level_color[m.msg_level] << _level_name[m.msg_level]
          << ENDCLR << ":" << YELLOW + m.module + ENDCLR << "--"
          << LIGHT_YELLOW + Time::isoDateTime(m.msg_time) + ENDCLR << "--"
          << m.msg;

        os << s.str() << endl;
    }

    log_t::log_t(std::string mod)
    {
        _module = mod;
        _pid = getpid();
    }

    void log_t::do_rest(log_t::Message &m)
    {
        m.msg = m.s.str();

        for (auto b: backends)
        {
            b->output(m);
        }
    }

    void log_t::set_log_level(int l)
    {
        _log_level = l;
    }

    void log_t::add_backend(std::shared_ptr<Backend> be)
    {
        backends.push_front(be);
    }

    void log_t::clear_backends()
    {
        backends.clear();
    }
}
