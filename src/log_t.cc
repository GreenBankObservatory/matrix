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
#if !defined(VXWORKS) // this class uses templates and STL

#include "matrix/log_t.h"

// for stat
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <vector>
#include <tuple>

using namespace std;
//using namespace Threads;

static pthread_t thread_create(void *(*fptr)(void *), void *data)
{
    pthread_t id;

    int result = pthread_create(&id, 0, fptr, data);
    if (result != 0 || id == 0)
    {   
        cout << "thread_create: id=" << id << " error="
             <<  result << endl;
    }
    return(id);
}


namespace matrix
{

    Levels log_t::_log_level{Levels::INFO_LEVEL};
    list<std::shared_ptr<log_t::Backend>> log_t::backends;

    int ostreamBackend::_ref{0};
    tsemfifo<string> ostreamBackend::_output_fifo;
    pthread_t ostreamBackend::_task;

    map<Levels, string> log_t::_level_name
    {
        {Levels::PRINT_LEVEL, "PRINT"},
        {Levels::FATAL_LEVEL, "FATAL"},
        {Levels::ERROR_LEVEL, "ERROR"},
        {Levels::WARNING_LEVEL, "WARNING"},
        {Levels::INFO_LEVEL, "INFO"},
        {Levels::DEBUG_LEVEL, "DEBUG"}
    };

    ostreamBackend::ostreamBackend(std::ostream &s)
    {
        if (_ref == 0)
        {
            _task = thread_create(_output_task, (void *)&s);
            _ref++;
        }
    }

    ostreamBackend::~ostreamBackend()
    {
        _ref--;
        cerr << "ostreamBackend destructor called, _ref = " << _ref << endl;

        if (_ref == 0)
        {
            if (_task != 0)
            {
                cerr << "releasing FIFO and cancelling thread." << endl;
                _output_fifo.release();
                pthread_join(_task, 0);
                _task = 0;
            }
        }
    }

    void ostreamBackend::output(LogMessage &m)
    {
        string msg;
        stringstream s;

        if (m.msg_level == Levels::PRINT_LEVEL)
        {
            s << m.msg();
        }
        else
        {
            s << log_t::level_name(m.msg_level) << ":" << m.module << "--"
              << Time::isoDateTime(m.msg_time) << "--" << m.msg();
        }

        msg = s.str();
        _output_fifo.put(msg);

    }

    void *ostreamBackend::_output_task(void *data)
    {
        string mesg;
        std::ostream &os = *(ostream *)data;

        while (_output_fifo.get(mesg))
        {
            os << mesg << endl;
        }

        cerr << __PRETTY_FUNCTION__ << " terminating." << endl;
        return NULL;
    }

    ostreamBackendColor::ostreamBackendColor(std::ostream &s)
        : ostreamBackend(s)
    {
        _level_color[Levels::DEBUG_LEVEL]   = LIGHT_CYAN;
        _level_color[Levels::INFO_LEVEL]    = LIGHT_GREEN;
        _level_color[Levels::WARNING_LEVEL] = MAGENTA;
        _level_color[Levels::ERROR_LEVEL]   = LIGHT_RED;
        _level_color[Levels::FATAL_LEVEL]   = RED;
    }

    ostreamBackendColor::~ostreamBackendColor()
    {
    }

    void ostreamBackendColor::output(LogMessage &m)
    {
        stringstream s;
        string msg;


        if (m.msg_level == Levels::PRINT_LEVEL)
        {
            s << m.msg();
        }
        else
        {
            s << _level_color[m.msg_level] << log_t::level_name(m.msg_level)
              << ENDCLR << ":" << YELLOW + m.module + ENDCLR << "--"
              << LIGHT_YELLOW + Time::isoDateTime(m.msg_time) + ENDCLR << "--"
              << m.msg();
        }

        msg = s.str();
        _output_fifo.put(msg);
    }

    log_t::log_t(string mod)
    {
        _module = mod;
    }

    void log_t::do_rest(LogMessage &m)
    {
        for (auto b: backends)
        {
            b->output(m);
        }
    }

    void log_t::set_log_level(Levels l)
    {
        _log_level = l;
    }

    void log_t::add_backend(shared_ptr<log_t::Backend> be)
    {
        backends.push_front(be);
    }

    string log_t::level_name(Levels l)
    {
        return _level_name[l];
    }

    void log_t::set_default_backend()
    {
        std::shared_ptr<log_t::Backend> backend;

        if (isatty(fileno(stdout)))
        {
            backend.reset(new ostreamBackendColor(cout));
        }
        else
        {
            backend.reset(new ostreamBackend(cout));
        }

        log_t::add_backend(backend);
    }
}

#endif
