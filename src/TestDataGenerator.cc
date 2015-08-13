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

#include "TestDataGenerator.h"
#include "matrix_util.h"

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
 * Constructor to the data_source_info nested structure, which
 * contains a description of the data to be output for a source.
 *
 */

    TestDataGenerator::data_source_info::data_source_info()
    {
        type_info["int8_t"] = sizeof(int8_t);
        type_info["uint8_t"] = sizeof(uint8_t);
        type_info["int16_t"] = sizeof(int16_t);
        type_info["uint16_t"] = sizeof(uint16_t);
        type_info["int32_t"] = sizeof(int32_t);
        type_info["uint32_t"] = sizeof(uint32_t);
        type_info["int64_t"] = sizeof(int64_t);
        type_info["uint64_t"] = sizeof(uint64_t);
        type_info["char"] = sizeof(char);
        type_info["unsigned char"] = sizeof(unsigned char);
        type_info["short"] = sizeof(short);
        type_info["unsigned short"] = sizeof(unsigned short);
        type_info["int"] = sizeof(int);
        type_info["unsigned"] = sizeof(unsigned);
        type_info["unsigned int"] = sizeof(unsigned int);
        type_info["long"] = sizeof(long);
        type_info["unsigned long"] = sizeof(unsigned long);
        type_info["bool"] = sizeof(bool);
        type_info["float"] = sizeof(float);
        type_info["double"] = sizeof(double);
        type_info["long double"] = sizeof(long double);
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
 * Adds a field description to the fields list in the data_source_info object. Field
 * descriptions consist of a vector<string> of the form
 *
 *      [field_name, field_type, initial_val]
 *
 * The fields list describes the data structure.
 *
 * @param f: the vector containing the description for the field.
 *
 */

    void TestDataGenerator::data_source_info::add_field(std::vector<std::string> &f)
    {
        data_source_info::data_field df;

        df.name = f[0];
        df.type = f[1];
        df.initial_val = f[2];

        fields.push_back(df);
    }

/**
 * Computes the size of the total buffer needed for the data source,
 * and the offset of the various fields. gcc in the x86_64
 * architecture stores structures in memory as a multiple of the
 * largest type of field in the structure. Thus, if the largest
 * element is an int, the size will be a multiple of 4; if it is a
 * long, of 8; etc. All elements must be stored on boundaries that
 * respect their size: int16_t on two-byte boundaries, int on 4-byte
 * boundaries, etc. Padding is added to make this work.
 *
 * case of long or double as largest field:
 *
 *     ---------------|---------------
 *               8 - byte value
 *     ---------------|---------------
 *       4-byte val   | 4-byte val
 *     ---------------|---------------
 *       4-byte val   |  padding (pd)
 *     ---------------|---------------
 *               8 - byte value
 *     ---------------|---------------
 *     1BV|1BV|  2BV  | 4-byte val
 *     ---------------|---------------
 *     1BV|    pd     | 4-byte val
 *
 *              etc...
 *
 * case of int or float being largest field:
 *
 *     ---------------
 *      4-byte value
 *     ---------------
 *      2BV   |1BV|pd
 *     ---------------
 *      1BV|pd| 2BV
 *     ---------------
 *      4-byte value
 *     ---------------
 *      2BV   | pd
 *
 *        etc...
 *
 * Structures that contain only one field are the size of that field
 * without any padding, since that one element is the largest element
 * type. So for a struct foo_t {int16_t i16;};, sizeof(foo_t) would be
 * 2.
 *
 * As it is computing the size this function also saves the offsets
 * into the various 'data_field' structures so that the data may be
 * properly accessed later.
 *
 * @return A size_t which is the size that the GenericBuffer should be
 * set to in order to contain the data properly.
 *
 */

    size_t TestDataGenerator::data_source_info::size()
    {
        // storage element size, offset in element, number of elements
        // used.
        size_t s_elem_size, offset(0), s_elems(1);

        // find largest element in structure.
        std::list<data_field>::iterator i = 
            max_element(fields.begin(), fields.end(),
                        [this](data_field &i, data_field &j)
                        {
                            return type_info[i.type] < type_info[j.type];
                        });
        s_elem_size = type_info[i->type];

        // compute the offset and multiples of largest element
        for (list<data_field>::iterator i = fields.begin(); i != fields.end(); ++i)
        {
            size_t s(type_info[i->type]);
            offset += offset % s;
            
            if (s_elem_size - offset < s)
            {
                offset = 0;
                s_elems++;
            }
            
            i->offset = s_elem_size * (s_elems - 1) + offset;
            offset += s;
        }
        
        return s_elem_size * s_elems;
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
                shared_ptr<DataSource<GenericBuffer> > gb(
                    new DataSource<GenericBuffer>(keymaster_url, my_instance_name, i->first));

                sources[i->first] = gb;
                data_source_info dsi;
                string periodic = yn.node["standins"][i->first]["periodic"].as<string>();
                dsi.interval = _parse_interval(periodic);

                YAML::Node fields = yn.node["standins"][i->first]["fields"];

                if (fields.IsSequence())
                {
                    vector<vector<string> > vs = fields.as<vector<vector<string> > >();

                    for (vector<vector<string> >::iterator k = vs.begin(); k != vs.end(); ++k)
                    {
                        dsi.add_field(*k);
                    }

                    data_specs[i->first] = dsi;
                }
                else if (fields.IsMap())
                {
                    map<string, vector<string> > vs = fields.as<map<string, vector<string> > >();

                    for (int i = 0; i < vs.size(); ++i)
                    {
                        std::string s = std::to_string(i);
                        dsi.add_field(vs[s]);
                    }

                    data_specs[i->first] = dsi;
                }
                else
                {
                    ostringstream msg;
                    msg << "Unable to convert YAML input " << fields << "Neither vector or map.";
                    throw MatrixException("TestDataGenerator::_read_source_information()",
                                          msg.str());
                }
            }
        }
        catch (YAML::Exception e)
        {
            ostringstream msg;
            msg << "Unable to convert YAML input " << e.what();
            throw MatrixException("TestDataGenerator::_read_source_information()", msg.str());
        }


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

        l.lock();
        boost::split(parts, key, boost::is_any_of("."));
        data_source_info dsi, &old_dsi = data_specs[parts.back()];

        try
        {
            dsi.interval = _parse_interval(n["periodic"].as<string>());

            YAML::Node fields = n["fields"];

            if (fields.IsSequence())
            {
                vector<vector<string> > vs = fields.as<vector<vector<string> > >();

                for (vector<vector<string> >::iterator k = vs.begin(); k != vs.end(); ++k)
                {
                    dsi.add_field(*k);
                }
            }
            else if (fields.IsMap())
            {
                map<string, vector<string> > vs = fields.as<map<string, vector<string> > >();

                for (int i = 0; i < vs.size(); ++i)
                {
                    std::string s = std::to_string(i);
                    dsi.add_field(vs[s]);
                }
            }
            else
            {
                // problem. Just do nothing and return
                cout << "TestDataGenerator::data_configuration_changed(): node neither map nor sequence." << endl
                     << n << endl;
            }

            // make sure all types match the old types. We cannot change
            // that while running.
            if (equal(dsi.fields.begin(), dsi.fields.end(), old_dsi.fields.begin(),
                      [] (data_source_info::data_field &a, data_source_info::data_field &b)
                      { return a.type == b.type;}))
            {
                data_specs[parts.back()] = dsi;
                test_data[parts.back()] = dsi.compute_generic_buffer();
            }
            else
            {
                cout << "Cannot change types while in \"Running\" state." << endl;
            }
        }
        catch (YAML::Exception e)
        {
            // problem. Just do nothing and return
            cout << "TestDataGenerator::data_configuration_changed(): Malformed node." << endl
                 << n << endl;
        }
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

        for (std::map<std::string, data_source_info>::iterator i = data_specs.begin();
             i != data_specs.end(); ++i)
        {
            GenericBuffer gb = i->second.compute_generic_buffer();
            test_data[i->first] = gb;
        }
    }

/**
 * This function, a member of data_source_info, creates a
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

    GenericBuffer TestDataGenerator::data_source_info::compute_generic_buffer()
    {
        GenericBuffer gb;
        size_t offset = 0;

        // size() computes the size and also the offsets of the
        // data. The latter are stored as the 'offset' field in the
        // data_field element type of 'fields'. See above.
        gb.resize(size());

        // for each element in 'fields', create the appropriate
        // pointer and store the initial value in the proper place in
        // the buffer.
        for (std::list<data_field>::iterator i = fields.begin(); i != fields.end(); ++i)
        {
            string ft = i->type;
            string v = i->initial_val;
            size_t o = i->offset;

            if (ft == "int8_t" || ft == "char")
            {
                int8_t *p = (int8_t *)(gb.data() + o);
                *p = (int8_t)convert<int>(v);
            }
            else if (ft == "uint8_t" || ft == "unsigned char")
            {
                uint8_t *p = (uint8_t *)(gb.data() + o);
                *p = (uint8_t)convert<unsigned int>(v);
            }
            else if (ft == "int16_t" || ft == "short")
            {
                int16_t *p = (int16_t *)(gb.data() + o);
                *p = (int16_t)convert<int>(v);
            }
            else if (ft == "uint16_t" || ft == "unsigned short")
            {
                uint16_t *p = (uint16_t *)(gb.data() + o);
                *p = (uint16_t)convert<unsigned int>(v);
            }
            else if (ft == "int32_t" || ft == "int")
            {
                int32_t *p = (int32_t *)(gb.data() + o);
                *p = (int32_t)convert<int>(v);
            }
            else if (ft == "uint32_t" || ft == "unsigned")
            {
                uint32_t *p = (uint32_t *)(gb.data() + o);
                *p = (uint32_t)convert<unsigned int>(v);
            }
            else if (ft == "int64_t" || ft == "long")
            {
                int64_t *p = (int64_t *)(gb.data() + o);
                *p = convert<long>(v);
            }
            else if (ft == "uint64_t" || ft == "unsigned long")
            {
                uint64_t *p = (uint64_t *)(gb.data() + o);
                *p = convert<unsigned long>(v);
            }
            else if (ft == "bool")
            {
                bool *p = (bool *)(gb.data() + o);
                *p = convert<bool>(v);
            }
            else if (ft == "float")
            {
                float *p = (float *)(gb.data() + o);
                *p = convert<float>(v);
            }
            else if (ft == "double")
            {
                double *p = (double *)(gb.data() + o);
                *p = convert<double>(v);
            }
        }

        return gb;
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
