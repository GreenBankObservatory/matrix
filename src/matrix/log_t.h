/*******************************************************************
 *  log_t.h - Simple and versatile multi-level logger.
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

#if !defined(_MATRIX_LOG_T_H_)
#define _MATRIX_LOG_T_H_

#include "matrix/Time.h"
#include "matrix/Thread.h"
#include "matrix/tsemfifo.h"
#include <string>
#include <map>
#include <list>
#include <iostream>
#include <sstream>
#include <memory>

namespace matrix
{
    enum struct Levels: int
    {
        PRINT_LEVEL = 0,
        FATAL_LEVEL,
        ERROR_LEVEL,
        WARNING_LEVEL,
        INFO_LEVEL,
        DEBUG_LEVEL
    };

    struct LogMessage
    {
        std::string msg() {return s.str();}

        Time::Time_t msg_time;
        Levels msg_level;
        std::string module;
        std::stringstream s;
    };


    class log_t
    {
    public:

        struct Backend
        {
            virtual void output(LogMessage &m) = 0;
        };

        log_t(std::string mod);

        template <typename T, typename... Args>
        void fatal(const T &rv, Args... args)
        {
            LogMessage m;

            if (preamble(m, Levels::FATAL_LEVEL, rv))
            {
                do_rest(m, args...);
            }
        }

        template <typename T, typename... Args>
        void error(const T &rv, Args... args)
        {
            LogMessage m;

            if (preamble(m, Levels::ERROR_LEVEL, rv))
            {
                do_rest(m, args...);
            }
        }

        template <typename T, typename... Args>
        void warning(const T &rv, Args... args)
        {
            LogMessage m;

            if (preamble(m, Levels::WARNING_LEVEL, rv))
            {
                do_rest(m, args...);
            }
        }

        template <typename T, typename... Args>
        void info(const T &rv, Args... args)
        {
            LogMessage m;

            if (preamble(m, Levels::INFO_LEVEL, rv))
            {
                do_rest(m, args...);
            }
        }

        template <typename T, typename... Args>
        void debug(const T &rv, Args... args)
        {
            LogMessage m;

            if (preamble(m, Levels::DEBUG_LEVEL, rv))
            {
                do_rest(m, args...);
            }
        }

        template <typename T, typename... Args>
        void print(const T &rv, Args... args)
        {
            LogMessage m;

            if (preamble(m, Levels::PRINT_LEVEL, rv))
            {
                do_rest(m, args...);
            }
        }

        static void set_log_level(Levels l = Levels::INFO_LEVEL);
        static void add_backend(std::shared_ptr<Backend>);
        static void set_default_backend();
        static std::string level_name(Levels l);

    private:

        template<typename T, typename... Args>
        void do_rest(LogMessage &m, T &a, Args... args)
        {
            m.s  << a;
            do_rest(m, args...);
        }

        // recursion end
        void do_rest(LogMessage &m);

        template<typename T>
        bool preamble(LogMessage &m, Levels level, T &rv)
        {
            bool rval = false;

            if (_log_level >= level)
            {
                m.s << rv;
                m.msg_level = level;
                m.msg_time = Time::getUTC();
                m.module = _module;

                rval = true;
            }

            return rval;
        }

        std::string _module;

        static Levels _log_level;
        static std::map<Levels, std::string> _level_name;
        static std::list<std::shared_ptr<Backend>> backends;
    };

    /**
     * \class Backend
     *
     * The Backend base class allows a caller to customize how the
     * message is delivered. The two classes instantiated here,
     * ostreamBackend and ostreamBackendColor, are working examples of
     * backends that provide a way to output to an ostream object, so
     * that one may output messages to the screen, to a file,
     * etc. Classes may also be created to send the logs to redis,
     * publish them via ZMQ, or anything one can think of.
     *
     */

    struct ostreamBackend : public log_t::Backend
    {
        ostreamBackend(std::ostream &s);
        virtual ~ostreamBackend();
        virtual void output(LogMessage &m);

    protected:

        static int _ref;
        static tsemfifo<std::string> _output_fifo;
        static pthread_t _task;
        static void * _output_task(void *);
    };

    struct ostreamBackendColor : public ostreamBackend
    {
        ostreamBackendColor(std::ostream &s);
        virtual ~ostreamBackendColor();
        virtual void output(LogMessage &m);

    private:

        std::map<Levels, std::string> _level_color;

        std::string RED{"\e[31m"};
        std::string YELLOW{"\e[33m"};
        std::string MAGENTA{"\e[35m"};
        std::string LIGHT_RED{"\e[91m"};
        std::string LIGHT_GREEN{"\e[92m"};
        std::string LIGHT_YELLOW{"\e[93m"};
        std::string LIGHT_CYAN{"\e[96m"};
        std::string ENDCLR{"\e[0m"};
    };
}

#endif
