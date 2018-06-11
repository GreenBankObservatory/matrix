/*******************************************************************
 *  TestDataGenerator.cc - Implementation of a dynamic test data
 *  generator. This component may act as a standin for upstream
 *  components that generate needed data to test the downstream
 *  component, allowing the downstream component to be tested even
 *  when the upstream component(s) have not yet been developed.
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

#include "matrix/TestDataGenerator.h"
#include "matrix/matrix_util.h"
#include "matrix/DataInterface.h"

#include <string>
#include <map>

using namespace std;
using namespace mxutils;

namespace matrix
{
 
    /**
     * Finds the smallest value of timeval in the map passed
     * into it, and returns the name of the key to that timeval.
     *
     * @param m: the timeval map. Keys to the map are the data source
     * names for this component.
     *
     * @return a std::string containing the key to the timeval.
     *
     */
    
    string next(map<string, timeval> &m)
    {
        timeval t = {0, 0};
        string name;

        for (map<string, timeval>::iterator i = m.begin(); i != m.end(); ++i)
        {
            if (t.tv_sec == 0 && t.tv_usec == 0)
            {
                t = i->second + 1.0;
            }

            if (i->second < t)
            {
                t = i->second;
                name = i->first;
            }
        }

        return name;
    }

/**
 * The factory for the TestDataGenerator component.
 *
 * @param name: the name of the component
 *
 * @param km_url: the keymaster url, for access to the keymaster.
 *
 * @return A Component * to the newly constructed component.
 *
 */

    Component *TestDataGenerator::factory(std::string name, std::string km_url)
    {
        return new TestDataGenerator(name, km_url);
    }

/**
 * The TestDataGenerator constructor. Constructs the component.
 *
 * @param name: the name of the component
 *
 * @param km_url: the keymaster url, for access to the keymaster.
 *
 */

    TestDataGenerator::TestDataGenerator(std::string name, std::string km_url)
        : Component(name, km_url),
          poll_thread(this, &TestDataGenerator::poll),
          poll_thread_started(false),
          _run(true)
    {
        _read_source_information();
    }

/**
 * TestDataGenerator destructor. Gets rid of any remaining sources and
 * their descriptions.
 *
 */

    TestDataGenerator::~TestDataGenerator()
    {
        _clean_up_sources();
    }

/**
 * The TestDataGenerator's "Running" thread entry point. It will
 * publish all specified sources at their appropriate intervals.
 *
 * The number of iterations through the loop will be stashed into the
 * keymaster under my_full_instance_name + ".poll_iterations" while
 * the thread is running.
 */

    void TestDataGenerator::poll()
    {
        poll_thread_started.signal(true);
        map<string, timeval> sleepytime;
        map<string, timeval>::iterator sleep_iter;
        timeval t, now;
        string name;
        ThreadLock<Mutex> l(_data_mutex);
        string iter_key = my_full_instance_name + ".poll_iterations";

        for (test_data_t::iterator i = test_data.begin(); i != test_data.end(); ++i)
        {
            string key = my_full_instance_name + ".standins." + i->first;
            keymaster->subscribe(key, new KeymasterMemberCB<TestDataGenerator>(
                                     this, &TestDataGenerator::data_configuration_changed));
        }

        gettimeofday(&now, NULL);

        for (data_specs_t::iterator i = data_specs.begin(); i != data_specs.end(); ++i)
        {
            t = now + i->second.interval;
            sleepytime[i->first] = now + i->second.interval;
        }

        // still need 'next()' and 'operator-()' for timeval.

        while (_run)
        {
            gettimeofday(&now, NULL);
            // gets next one due
            name = next(sleepytime);
            // sleep until then
            if ((sleep_iter = sleepytime.find(name)) != sleepytime.end())
            {
                t = sleep_iter->second - now;
            }
            else
            {
                t = {1, 0};
            }

            do_nanosleep(t.tv_sec, t.tv_usec * 1000);
            // publish the data

            if (sources.find(name) != sources.end())
            {
                l.lock();
                sources[name]->publish(test_data[name]);
                l.unlock();

                gettimeofday(&now, NULL);
                sleepytime[name] = now + data_specs[name].interval;
            }
        }

        for (test_data_t::iterator i = test_data.begin(); i != test_data.end(); ++i)
        {
            string key = my_full_instance_name + ".standins." + i->first;
            keymaster->unsubscribe(key);
        }

        keymaster->del(iter_key);
        cout << "poll(): thread terminated." << endl;
    }

/**
 * Parses the user input for the period data for a data source. Period
 * data may be entered as values with suffixes:
 *
 *      20.5mS
 *      50mS
 *      500000uS
 *
 * If no suffix is provided, assumes the value is in seconds.
 *
 * @param interval: The period description
 *
 * @return a floating point value in seconds.
 *
 */

    double TestDataGenerator::_parse_interval(string interval)
    {
        string ms = "ms", us = "us";
        double multiplier(1.0);
        double rval;

        transform(interval.begin(), interval.end(), interval.begin(), ::tolower);

        if (interval.find(ms) != string::npos)
        {
            multiplier = 0.001;
        }
        else if (interval.find(us) != string::npos)
        {
            multiplier = 1e-6;
        }

        rval = convert<double>(interval) * multiplier;
        return rval;
    }

/**
 * Reads the data description for the component's sources from the
 * keymaster, building data_source_info objects with that data, and
 * creates the data buffers that will be published based on these
 * descriptions.
 *
 */

    void TestDataGenerator::_read_source_information()
    {
        string looking_for;
        yaml_result yn;

        try
        {
            looking_for = my_full_instance_name;
            keymaster->get(looking_for, yn);
            map<string, string> srcs = yn.node["Sources"].as<map<string, string> >();

            for (map<string, string>::iterator i = srcs.begin(); i != srcs.end(); ++i)
            {
                // set up the DataSources:
                shared_ptr<DataSource<GenericBuffer> > gb(
                    new DataSource<GenericBuffer>(keymaster_url, my_instance_name, i->first));
                sources[i->first] = gb;
                // parse the data descriptions corresponding to this source
                _parse_data_description(i->first, yn.node["standins"][i->first]);
            }
        }
        catch (YAML::Exception &e)
        {
            ostringstream msg;
            msg << "Unable to convert YAML input " << e.what();
            throw MatrixException("TestDataGenerator::_read_source_information()", msg.str());
        }
    }

    bool TestDataGenerator::_parse_data_description(string name, YAML::Node desc)
    {
        bool rval = true;

        try
        {
            YAML:: Node fields = desc ["fields"];
            data_description dsi(fields);
            vector <string> dv;

            // Get the default values:
            if (fields.IsSequence())
            {
                vector<vector<string> > vs = fields.as<vector<vector<string> > >();

                for (vector<vector<string> >::iterator k = vs.begin(); k != vs.end(); ++k)
                {
                    dv.push_back((*k)[3]);
                }
            }
            else if (fields.IsMap())
            {
                map<string, vector<string> > vs = fields.as<map<string, vector<string> > >();

                for (size_t i = 0; i < vs.size(); ++i)
                {
                    std::string s = std::to_string(i);
                    dv.push_back(vs[s][3]);
                }
            }
            else
            {
                // problem. Just do nothing and return
                cout << "TestDataGenerator::_parse_data_description(): 'field' node neither map nor sequence." << endl
                     << fields << endl;
                return false;
            }

            // save the interval
            dsi. interval = _parse_interval (desc["periodic"].as<string>());
            // store the source data description
            data_specs [name] = dsi;
            // store the default values
            default_vals [name] = dv;
        }
        catch (YAML::Exception &e)
        {
            cerr << "TestDataGenerator::_parse_data_description(): YAML::Exception: " << e.what() << endl;
            rval = false;
        }

        return rval;
    }


/**
 * Keymaster callback function. The publishing thread subscribes to
 * the Keymaster for notification of any change to the data
 * descriptions of each of the data sources that this Component
 * publishes. This is used so that the 'periodic' key and the
 * 'initial_value' keys in the field descriptors may be updated while
 * the component is 'Running.' Field names may also be modified but
 * will have no effect. Field types modifications are prohibited
 * however. If any type is modified the callback will do nothing and
 * simply return. If everything is in proper order then a new buffer
 * will be constructed based on the new description and the next time
 * that source becomes due it will be published.
 *
 * @param key: the key used to subscribe. It is in the form
 * 'my_full_instance_name + ".standins" + <source_name>
 *
 * @param n: the YAML node represented by the key.
 *
 */

    void TestDataGenerator::data_configuration_changed(std::string key, YAML::Node n)
    {
        ThreadLock<Mutex> l(_data_mutex);
        vector<string> parts;
        boost::split(parts, key, boost::is_any_of("."));
        string name(parts.back());

        l.lock();
        data_description old_dsi = data_specs[name];
        vector<string> defaults = default_vals[name];

        if (!_parse_data_description(name, n))
        {
            return;
        }

        data_description &dsi = data_specs[name];
        YAML::Node fields = n["fields"];

        // make sure all types match the old types. We cannot change
        // that while running.
        if (!equal(dsi.fields.begin(), dsi.fields.end(), old_dsi.fields.begin(),
                  [] (data_description::data_field &a, data_description::data_field &b)
                  { return a.type == b.type;}))
        {
            cout << "Cannot change types while in \"Running\" state." << endl;
            // restore old stuff
            data_specs[name] = old_dsi;
            default_vals[name] = defaults;
        }

        // good or bad, (re)create the generic test data buffer
        test_data[name] = _create_generic_buffer(default_vals[name], data_specs[name]);
    }

/**
 * This function, a member of data_description, creates a
 * GenericBuffer based on the data description for that data
 * source. Currently supports the following type names:
 *
 *   - int8_t, char, uint8_t, unsigned char, int16_t, short, uint16_t,
 *     unsigned short, int32_t, int, uint32_t, usigned, int64_t, long,
 *     uint64_t, unsigned long, bool, float, double.
 *
 * @return  Using the type name and the initial values in the data description,
 * returns a GenericBuffer properly structured to match the gcc
 * compiler's structure packing.
 *
 */

    GenericBuffer TestDataGenerator::_create_generic_buffer(vector<string> &init_vals, data_description dd)
    {
        GenericBuffer gb;

        // size() computes the size and also the offsets of the
        // data. The latter are stored as the 'offset' field in the
        // data_field element type of 'fields'. See above.
        gb.resize(dd.size());

        // for each element in 'fields', create the appropriate
        // pointer and store the initial value in the proper place in
        // the buffer.

        int k = 0;

        for (std::list<data_description::data_field>::iterator i = dd.fields.begin();
             i != dd.fields.end(); ++i, ++k)
        {
            string v = init_vals[k];
            data_description::types ft = i->type;
            size_t o = i->offset;

            if (ft == data_description::INT8_T || ft == data_description::CHAR)
            {
                set_data_buffer_value(gb.data(), o, convert<int8_t>(v));
            }
            else if (ft == data_description::UINT8_T || ft == data_description::UNSIGNED_CHAR)
            {
                set_data_buffer_value(gb.data(), o, convert<uint8_t>(v));
            }
            else if (ft == data_description::INT16_T || ft == data_description::SHORT)
            {
                set_data_buffer_value(gb.data(), o, convert<int16_t>(v));
            }
            else if (ft == data_description::UINT16_T || ft == data_description::UNSIGNED_SHORT)
            {
                set_data_buffer_value(gb.data(), o, convert<uint16_t>(v));
            }
            else if (ft == data_description::INT32_T || ft == data_description::INT)
            {
                set_data_buffer_value(gb.data(), o, convert<int32_t>(v));
            }
            else if (ft == data_description::UINT32_T || ft == data_description::UNSIGNED_INT)
            {
                set_data_buffer_value(gb.data(), o, convert<uint32_t>(v));
            }
            else if (ft == data_description::INT64_T || ft == data_description::LONG)
            {
                set_data_buffer_value(gb.data(), o, convert<int64_t>(v));
            }
            else if (ft == data_description::UINT64_T || ft == data_description::UNSIGNED_LONG)
            {
                set_data_buffer_value(gb.data(), o, convert<uint64_t>(v));
            }
            else if (ft == data_description::BOOL)
            {
                set_data_buffer_value(gb.data(),o,convert<bool>(v));
            }
            else if (ft == data_description::FLOAT)
            {
                set_data_buffer_value(gb.data(), o, convert<float>(v));
            }
            else if (ft == data_description::DOUBLE)
            {
                set_data_buffer_value(gb.data(), o, convert<double>(v));
            }
        }

        return gb;
    }


/**
 * Generates GenericBuffer objects for all sources based on the data
 * description of those sources and stores them in a map. The keys to
 * that map are the names of the data sources.
 *
 */

    void TestDataGenerator::_create_test_data_buffers()
    {
        test_data.clear();

        for (std::map<std::string, data_description>::iterator i = data_specs.begin();
             i != data_specs.end(); ++i)
        {
            GenericBuffer gb = _create_generic_buffer(default_vals[i->first], i->second);
            test_data[i->first] = gb;
        }
    }
    
/**
 * Clears out the sources, data specs, and test data buffer containers.
 *
 */

    void TestDataGenerator::_clean_up_sources()
    {
        sources.clear();
        data_specs.clear();
        test_data.clear();
    }

/**
 * Called on the transition from 'Standby' to 'Ready'. This is when
 * the sources should be created (since they should exist while the
 * component is 'Ready').
 *
 * @return true on success, false otherwise.
 *
 */

    bool TestDataGenerator::_do_ready()
    {
        cout << "_do_ready()" << endl;
        _read_source_information();
        _create_test_data_buffers();

        return true;
    }

/**
 * Called on the transition from 'Ready' to 'Running'. Starts the
 * publishing thread running.
 *
 * @return true if the thread was successfully started, false
 * otherwise. The Component will transition back to ready if this
 * returns false.
 *
 */

    bool TestDataGenerator::_do_start()
    {
        if (!poll_thread.running())
        {
            cout << "_do_start(): starting thread." << endl;
            poll_thread.start();
            bool started = poll_thread_started.wait(true, 1000000);
            string state =  started ? string(" started") : string(" did not start");
            cout << "_do_start(): thread " << state << endl;
            return started;
        }

        return true;
    }

/**
 * Called on the transition from 'Running' to 'Ready'. Waits for the
 * publishing thread to exit gracefully.
 *
 * @return true on success, false otherwise.
 *
 */

    bool TestDataGenerator::_do_stop()
    {
        if (poll_thread.running())
        {
            cout << "_do_stop(): Stopping thread." << endl;
            _run = false;
            poll_thread.stop_without_cancel();
            cout << "_do_stop(): thread done." << endl;
        }

        poll_thread_started.set_value(false);
        _run = true;
        return true;
    }

/**
 * Called on the transition from 'Ready' to 'Standby'. Wipes out the
 * current set of sources, data specifications, and test data buffers.
 *
 * @return true on success, false otherwise.
 *
 */

    bool TestDataGenerator::_do_standby()
    {
        cout << "_do_standby()" << endl;
        _clean_up_sources();
        return true;
    }
   
}
