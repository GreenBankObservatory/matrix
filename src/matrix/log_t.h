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

#if !defined(_LOG_T_H_)
#define _LOG_T_H_

#include <matrix/Time.h>
#include <string>
#include <map>
#include <list>
#include <iostream>
#include <sstream>
#include <memory>
#include <sys/types.h>

namespace matrix
{

    class log_t
    {
    public:
        struct Backend;

        enum levels
        {
            FATAL_LEVEL = 0,
            ERROR_LEVEL,
            WARNING_LEVEL,
            INFO_LEVEL,
            DEBUG_LEVEL
        };

        struct Message
        {
            Message()
            {
                clear();
            }

            void clear()
            {
                msg.clear();
                msg_time = 0;
                msg_level = DEBUG_LEVEL;
                module.clear();
                pid = 0;
            }

            Message &operator=(Message const &rhs)
            {
                msg_time = rhs.msg_time;
                msg_level = rhs.msg_level;
                module = rhs.module;
                msg = rhs.msg;
                pid = rhs.pid;
                return *this;
            }

            std::string msg;
            Time::Time_t msg_time{0};
            levels msg_level{DEBUG_LEVEL};
            std::string module;
            std::stringstream s;
            pid_t pid{0};
        };

        log_t(std::string mod);

        template <typename T, typename... Args>
        void fatal(const T &rv, Args... args)
        {
            Message m;

            if (preamble(m, FATAL_LEVEL, rv))
            {
                do_rest(m, args...);
            }
        }

        template <typename T, typename... Args>
        void error(const T &rv, Args... args)
        {
            Message m;

            if (preamble(m, ERROR_LEVEL, rv))
            {
                do_rest(m, args...);
            }
        }

        template <typename T, typename... Args>
        void warning(const T &rv, Args... args)
        {
            Message m;

            if (preamble(m, WARNING_LEVEL, rv))
            {
                do_rest(m, args...);
            }
        }

        template <typename T, typename... Args>
        void info(const T &rv, Args... args)
        {
            Message m;

            if (preamble(m, INFO_LEVEL, rv))
            {
                do_rest(m, args...);
            }
        }

        template <typename T, typename... Args>
        void debug(const T &rv, Args... args)
        {
            Message m;

            if (preamble(m, DEBUG_LEVEL, rv))
            {
                do_rest(m, args...);
            }
        }

        static void set_log_level(int l = INFO_LEVEL);
        static void add_backend(std::shared_ptr<Backend>);
        static void clear_backends();

    private:

        template<typename T, typename... Args>
        void do_rest(Message &m, T &a, Args... args)
        {
            m.s << " " << a;
            do_rest(m, args...);
        }

        // recursion end
        void do_rest(Message &m);

        template<typename T>
        bool preamble(Message &m, levels level, T &rv)
        {
            bool rval = false;

            if (_log_level >= level)
            {
                m.s << rv;
                m.msg_level = level;
                m.msg_time = Time::getUTC();
                m.module = _module;
                m.pid = _pid;

                rval = true;
            }

            return rval;
        }

        std::string _module;
        pid_t _pid;

        static int _log_level;
        static std::list<std::shared_ptr<Backend>> backends;

    public:

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

        struct Backend
        {
            virtual void output(Message &m) = 0;
        };

        struct ostreamBackend : public Backend
        {
            ostreamBackend(std::ostream &s);
            virtual void output(Message &m);

        protected:
            std::map<levels, std::string> _level_name;
            std::ostream &os;
        };

        struct ostreamBackendColor : public ostreamBackend
        {
            ostreamBackendColor(std::ostream &s);
            virtual void output(Message &m);

        private:
            std::map<levels, std::string> _level_color;

            std::string RED{"\e[31m"};
            std::string YELLOW{"\e[33m"};
            std::string MAGENTA{"\e[35m"};
            std::string LIGHT_RED{"\e[91m"};
            std::string LIGHT_GREEN{"\e[92m"};
            std::string LIGHT_YELLOW{"\e[93m"};
            std::string LIGHT_CYAN{"\e[96m"};
            std::string ENDCLR{"\e[0m"};
        };
    };
}

#endif
