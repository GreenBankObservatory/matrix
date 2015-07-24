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

#include "Component.h"
#include "Thread.h"
#include "DataSource.h"
#include "DataInterface.h"

#include <string>
#include <list>
#include <map>

namespace matrix
{

    class TestDataGenerator : public Component
    {
    public:

        static Component *factory(std::string, std::string);
        virtual ~TestDataGenerator();

    protected:

        TestDataGenerator(std::string name, std::string km_url);

        void poll();

        virtual bool _do_ready();
        virtual bool _do_start();
        virtual bool _do_stop();
        virtual bool _do_standby();

        void data_configuration_changed(std::string, YAML::Node);

        struct data_source_info
        {
            struct data_field
            {
                std::string name;         // field name (needed?)
                std::string type;         // field type
                size_t offset;
                std::string initial_val;  // A value for the field
            };

            double interval; // in seconds
            std::list<data_field> fields;
            std::map<std::string, size_t> type_info;

            data_source_info();
            void add_field(std::vector<std::string> &f);
            size_t size();
            matrix::GenericBuffer compute_generic_buffer();
        };

        void _read_source_information();
        void _create_sources();
        void _clean_up_sources();
        double _parse_interval(std::string);
        void _create_test_data_buffers();

        // one or more sources, to be configured according to the
        // 'data_specs' for that source name.
        typedef std::map<std::string,
                         std::shared_ptr<matrix::DataSource<matrix::GenericBuffer> > > sources_t;
        typedef std::map<std::string, data_source_info> data_specs_t;
        typedef std::map<std::string, matrix::GenericBuffer> test_data_t;

        sources_t sources;
        data_specs_t data_specs;
        test_data_t test_data;

        Thread<TestDataGenerator> poll_thread;
        TCondition<bool>          poll_thread_started;
        bool                      _run;
        Mutex                     _data_mutex;
    };

}

#endif // _TESTDATAGENERATOR_H_
