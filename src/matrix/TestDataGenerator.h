/*******************************************************************
 *  TestDataGenerator.h - Declares a component that generates test
 *  data, based on data descriptions from the Keymaster.
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

#if !defined(_TESTDATAGENERATOR_H_)
#define _TESTDATAGENERATOR_H_

#include "matrix/Component.h"
#include "matrix/Thread.h"
#include "matrix/DataInterface.h"

#include <string>
#include <list>
#include <map>

namespace matrix
{
    class TestDataGenerator : public matrix::Component
    {
    public:

        static matrix::Component *factory(std::string, std::string);
        virtual ~TestDataGenerator();

    protected:

        TestDataGenerator(std::string name, std::string km_url);

        void poll();

        virtual bool _do_ready();
        virtual bool _do_start();
        virtual bool _do_stop();
        virtual bool _do_standby();

        void data_configuration_changed(std::string, YAML::Node);

        void _read_source_information();
        void _create_sources();
        void _clean_up_sources();
        double _parse_interval(std::string);
        bool _parse_data_description(std::string, YAML::Node);
        void _create_test_data_buffers();
        GenericBuffer _create_generic_buffer(std::vector<std::string> &, data_description);
        // one or more sources, to be configured according to the
        // 'data_specs' for that source name.
        typedef std::map<std::string,
                         std::shared_ptr<matrix::DataSource<matrix::GenericBuffer> > > sources_t;
        typedef std::map<std::string, data_description> data_specs_t;
        typedef std::map<std::string, std::vector<std::string> > default_vals_t;
        typedef std::map<std::string, matrix::GenericBuffer> test_data_t;

        sources_t sources;
        data_specs_t data_specs;
        test_data_t test_data;
        default_vals_t default_vals;

        matrix::Thread<TestDataGenerator> poll_thread;
        matrix::TCondition<bool>          poll_thread_started;
        bool                      _run;
        matrix::Mutex                     _data_mutex;
    };

}

#endif // _TESTDATAGENERATOR_H_
