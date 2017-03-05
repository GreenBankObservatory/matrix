/*******************************************************************
 *  GRTestComponent.cc - Implementation of a Component which
 *  can read generic data from a file, and publish it for processing.
 *
 *  Copyright (C) 2017 Associated Universities, Inc. Washington DC, USA.
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

#include "GRTestComponent.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <signal.h>
#include <cstdio>
#include <exception>
#include <cmath>
#include <sys/time.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "matrix/yaml_util.h"
#include "matrix/ResourceLock.h"
#include "GRTestComponent.h"

using namespace std;
using namespace Time;
using namespace mxutils;
using namespace matrix;

static void throw_value_error(string key, string msg)
{
    cerr << msg << " must be provided for the "
         << key << " keyword" << endl;
    cerr << "NOTE: this is considered a fatal error. Check Configuration"
         << endl;
    KeymasterException q("Key value invalid");
    throw q;
}

Component * GRTestComponent::factory(string name, string km_url)
{
    return new GRTestComponent(name, km_url);
}

GRTestComponent::GRTestComponent(string name, string km_url) :
    Component(name, km_url),
    data_source(km_url, my_instance_name, "block_data"),
    gr_src(km_url, my_instance_name, "grc_data"),
    _read_thread(this, &GRTestComponent::_reader_thread),
    _read_thread_started(false),
    _run(true),
    blocksize(0),
    filename(),
    repeat_continuously(true)
{

}


/// Disconnect and release resources.
GRTestComponent::~GRTestComponent()
{
}

void GRTestComponent::_reader_thread()
{
    FILE *fin;
    fin = fopen(filename.c_str(), "r");
    if (!fin)
    {
        cout << __PRETTY_FUNCTION__ << " unable to open file " << filename << endl;
        cout << strerror(errno) << endl;
        disconnect();
        _read_thread_started.signal(false);
        return; // TODO: not sure what to do here
    }
    ResourceLock fd_holder([&fin]()
                           {
                               cout << "closed FileReader file" << endl;
                               fclose(fin);
                               fin = nullptr;
                           } );
    _read_thread_started.signal(true);

    bool run = true;
    int nbytes;

    buffer.reset( new matrix::GenericBuffer() );
    buffer->resize(blocksize);

    timespec delay = {5, 0}; // 1 msec
    while (run)
    {
        clock_nanosleep(CLOCK_REALTIME, 0, &delay, nullptr);
        nbytes = fread(buffer->data(), blocksize, 1, fin);
        if (nbytes > 0)
        {
            try
            {
                data_source.publish(*buffer);
            }
            catch (MatrixException e)
            {
                cout << __PRETTY_FUNCTION__ << e.what() << endl;
                stop();
            }
        }
        else if (feof(fin) && repeat_continuously)
        {
            rewind(fin);
        }
        else
        {
            cout << __PRETTY_FUNCTION__ << " error on input file (too small perhaps?)" << endl;
            stop();
        }
        _run.get_value(run);
    }
}


bool GRTestComponent::connect()
{
    // Source only Components really don't need connect/disconnect
    yaml_result yr;
    if (keymaster->get(my_full_instance_name + ".filename", yr))
    {
        filename = yr.node.as<string>();
    }
    else
    {
        cout << __PRETTY_FUNCTION__ << " Invalid configuration "
        << " filename attribute is not present in config file" << endl;
        return false;
    }
    if (keymaster->get(my_full_instance_name + ".message_size", yr))
    {
        blocksize = yr.node.as<size_t>();
    }
    else
    {
        cout << __PRETTY_FUNCTION__ << " Invalid configuration "
        << " message_size attribute is not present in config file" << endl;
        return false;
    }
    struct stat st;
    // Does the file exist?
    if (stat(filename.c_str(), &st) != 0)
    {
        cout << __PRETTY_FUNCTION__ << " unable to stat file " << filename << endl;
        cout << strerror(errno) << endl;
        return(false);
    }
    // check size and warn
    if (st.st_size < blocksize)
    {
        cout << __PRETTY_FUNCTION__ << " file size is less than one block cannot continue" << endl;
        return false;
    }
    else if (st.st_size % blocksize)
    {
        cout << __PRETTY_FUNCTION__ << " file size is not multiple of blocksize -- some data will be skipped"
             << endl;
    }
    return true;
}

bool GRTestComponent::disconnect()
{
    // Source only Components really don't need connect/disconnect
    return true;
}

bool GRTestComponent::_stop()
{
    keymaster->put(my_full_instance_name + ".command", "stop");
    cout << __PRETTY_FUNCTION__ << " _stop()" << endl;
    return true;
}

bool GRTestComponent::_do_start()
{
    if (!connect())
    {
        return false;
    }

    if (!_read_thread.running())
    {
        cout << "GRTestComponent::_do_start(): starting thread." << endl;
        _run.set_value(true);
        _read_thread.start("FileReader");
    }

    _run.set_value(true);
    bool rval = _read_thread_started.wait(true, 5000000);

    if (rval)
    {
        cout << "GRTestComponent started." << endl;
    }
    else
    {
        cout << "GRTestComponent failed to start!" << endl;
        _run.set_value(false);
        if (_read_thread.running())
        {
            _read_thread.stop();
        }
    }

    return rval;
}

bool GRTestComponent::_do_stop()
{
    if (_read_thread.running())
    {
        cout << "GRTestComponent::_do_stop(): Killing thread." << endl;
        _run.signal(false);
        _read_thread.join();
        cout << "GRTestComponent dead." << endl;
    }

    _read_thread_started.set_value(false);
    _run.set_value(false);
    disconnect();
    return true;
}

