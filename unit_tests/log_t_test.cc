/*******************************************************************
 *  log_t_test.cc - Unit tests for the log_t logger module.
 *
 *  Copyright (C) 2018 Associated Universities, Inc. Washington DC, USA.
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
#include "matrix/matrix_util.h"
#include "log_t_test.h"
#include <sys/types.h>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string_regex.hpp>

using namespace std;
using namespace matrix;
using namespace mxutils;

ostream &operator << (ostream &o, log_t::Message const &m)
{
    o << "{"
      << "msg_time = " << Time::isoDateTime(m.msg_time)
      << ", msg_level = " << m.msg_level
      << ", module = " << m.module
      << ", pid = " << m.pid
      << ", msg = " << m.msg << "}";

    return o;
}

// This test backend gives us access to a log_t::Message structure.

struct test_backend : public log_t::Backend
{
    void output(log_t::Message &m)
    {
        message = m;
    }

    log_t::Message message;
};


void log_tTest::test_logger()
{
    log_t logger("test_logger");
    log_t::set_log_level(log_t::DEBUG_LEVEL);
    std::shared_ptr<test_backend> backend(new test_backend());
    log_t::add_backend(backend);
    vector<string> vs{"foo", "bar", "baz", "quux"};
    pid_t pid = getpid();

    Time::Time_t ts = Time::getUTC();
    logger.debug(__PRETTY_FUNCTION__, "This is a DEBUG message;", "vector vs =", vs);
    CPPUNIT_ASSERT(backend->message.pid == pid);
    CPPUNIT_ASSERT(backend->message.msg_time != 0);
    // Compare down to a second, still could fail once in a blue moon
    CPPUNIT_ASSERT(Time::isoDateTime(backend->message.msg_time).substr(0, 18)
                   == Time::isoDateTime(ts).substr(0, 18));
    CPPUNIT_ASSERT(backend->message.msg_level == log_t::DEBUG_LEVEL);
    CPPUNIT_ASSERT(backend->message.module == "test_logger");
    CPPUNIT_ASSERT(backend->message.msg.find("[foo, bar, baz, quux]") != string::npos);


    // Test the levels. We already tested debug above. Check the
    // string for a new message, because our simple backend gets
    // reused and the message is not cleared.
    log_t::set_log_level(log_t::WARNING_LEVEL);
    logger.debug(__PRETTY_FUNCTION__, "New Debug Message");
    CPPUNIT_ASSERT(backend->message.msg.find("New Debug") == string::npos);
    logger.info(__PRETTY_FUNCTION__, "New Info Message");
    CPPUNIT_ASSERT(backend->message.msg.find("New Info") == string::npos);
    logger.warning(__PRETTY_FUNCTION__, "New Warning Message");
    CPPUNIT_ASSERT(backend->message.msg.find("New Warning") != string::npos);
    logger.error(__PRETTY_FUNCTION__, "New Error Message");
    CPPUNIT_ASSERT(backend->message.msg.find("New Error") != string::npos);
    logger.fatal(__PRETTY_FUNCTION__, "New Fatal Message");
    CPPUNIT_ASSERT(backend->message.msg.find("New Fatal") != string::npos);
}

void log_tTest::test_ostream_backend()
{
    log_t logger("test_logger");
    log_t::set_log_level(log_t::DEBUG_LEVEL);
    stringstream s;
    std::shared_ptr<log_t::Backend> ostream_be(new log_t::ostreamBackend(s));
    log_t::clear_backends();
    log_t::add_backend(ostream_be);

    logger.info(__PRETTY_FUNCTION__, "New Info Message");
    string msg = s.str();
    vector<string> parts;
    boost::algorithm::split_regex(parts, msg, boost::regex("--"));
    CPPUNIT_ASSERT(parts.size() == 3);
    vector<string> parts2;
    boost::split(parts2, parts[0], boost::is_any_of(":"));
    CPPUNIT_ASSERT(parts2.size() == 2);
    CPPUNIT_ASSERT(parts2[0] == "INFO");
    CPPUNIT_ASSERT(parts2[1] == "test_logger");
    CPPUNIT_ASSERT(parts[2].find("New Info") != string::npos);
}

void log_tTest::test_ostream_color_backend()
{
    log_t logger("test_logger");
    log_t::set_log_level(log_t::DEBUG_LEVEL);
    stringstream s;
    std::shared_ptr<log_t::Backend> ostream_be(new log_t::ostreamBackendColor(s));
    log_t::clear_backends();
    log_t::add_backend(ostream_be);

    logger.info(__PRETTY_FUNCTION__, "New Info Message");
    string msg = s.str();
    vector<string> parts;
    boost::algorithm::split_regex(parts, msg, boost::regex("--"));
    CPPUNIT_ASSERT(parts.size() == 3);
    vector<string> parts2;
    boost::split(parts2, parts[0], boost::is_any_of(":"));
    CPPUNIT_ASSERT(parts2.size() == 2);
    // The string comparison should fail because there is color info embedded
    CPPUNIT_ASSERT(parts2[0] != "INFO");
    // The string search should succeed because INFO is in there.
    CPPUNIT_ASSERT(parts2[0].find("INFO") != string::npos);
    // Ditto for the module.
    CPPUNIT_ASSERT(parts2[1] != "test_logger");
    CPPUNIT_ASSERT(parts2[1].find("test_logger") != string::npos);
    CPPUNIT_ASSERT(parts[2].find("New Info") != string::npos);
}
