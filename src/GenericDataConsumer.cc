/*******************************************************************
 *  GenericDataConsumer.cc - Implementation of the generic consumer
 *  component. This component will read the Keymaster and set up to
 *  receive the data described by the Keymaster in such a way that it
 *  is compatible with the source. Will abort if the source and
 *  destination sizes don't match.
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

#include "matrix/GenericDataConsumer.h"
#include "matrix/yaml_util.h"
#include "matrix/Keymaster.h"
#include "matrix/Component.h"

// #include <stdio.h>
// #include <stdlib.h>
// #include <math.h>
// #include <cstdio>
// #include <exception>
// #include <iostream>

using namespace std;
using namespace mxutils;

namespace matrix
{

    static void throw_value_error(string key, string msg)
    {
        cerr << msg << " must be provided for the "
             << key << " keyword" << endl;
        cerr << "NOTE: this is considered a fatal error. Check Configuration"
             << endl;
        KeymasterException q("Key value invalid");
        throw q;
    }

    Component * GenericDataConsumer::factory(string name, string km_url)
    {
        return new GenericDataConsumer(name, km_url);
    }

    GenericDataConsumer::GenericDataConsumer(string name, string km_url) :
        Component(name, km_url),
        _sink(km_url, 100),
        _thread(this, &GenericDataConsumer::_task),
        _thread_started(false),
        _run(true),
        _handler(new GenericBufferHandler())
    {
    }

    GenericDataConsumer::~GenericDataConsumer()
    {
    }

    void GenericDataConsumer::_task()
    {
        bool run(true);
        Keymaster km(keymaster_url);
        GenericBuffer data;
        YAML::Node dd;

        try
        {
            dd = km.get(my_full_instance_name + ".data_description");
        }
        catch (KeymasterException &e)
        {
            cerr << e.what() << endl;
            throw_value_error(my_full_instance_name + ".data_description", e.what());
        }

        _thread_started.signal(true);

        while (run)
        {
            // try to get with a time-out of 5 mS
            if (_sink.timed_get(data, 5000000))
            {
                if (_handler)
                {
                    _handler->exec(dd, data);
                }
            }

            // continue until _run is false and no heaps were read.
            _run.get_value(run);
        }
    }

    bool
    GenericDataConsumer::connect()
    {
        // find the src component and sourcename for our sink in this mode
        connect_sink(_sink, "data_in");
        return true;
    }

    bool
    GenericDataConsumer::disconnect()
    {
        _sink.disconnect();
        return true;
    }

    bool
    GenericDataConsumer::_do_start()
    {
        connect();

        if (!_thread.running())
        {
            _thread.start("generic_consumer");
        }

        _thread_started.wait(true); // should really wait with timeout...
        return true;
    }

    bool
    GenericDataConsumer::_do_stop()
    {
        if (_thread.running())
        {
            _run = false;
            _thread.stop_without_cancel();
        }

        _thread_started.set_value(false);
        _run = true;
        disconnect();
        return true;
    }
}
