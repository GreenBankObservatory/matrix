/*******************************************************************
 *  FileDataSink.cc - Implementation of a Component which
 *  can write generic data from a file, and publish it for processing.
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

#include "FileDataSink.h"

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

matrix::Component * FileDataSink::factory(string name, string km_url)
{
    return new FileDataSink(name, km_url);
}

FileDataSink::FileDataSink(string name, string km_url) :
    Component(name, km_url),
    data_sink(km_url, 10),
    _write_thread(this, &FileDataSink::_writer_thread),
    _write_thread_started(false),
    _run(true),
    blocksize(0),
    filename()
{

}


/// Disconnect and release resources.
FileDataSink::~FileDataSink()
{
}

void FileDataSink::_writer_thread()
{
    FILE *fout;

    _write_thread_started.signal(true);

    fout = fopen(filename.c_str(), "w+");
    if (!fout)
    {
        cout << __PRETTY_FUNCTION__ << " unable to open file " << filename << endl;
        stop();
    }

    ResourceLock fd_holder([&fout]()
                           {
                               cout << "closed FileWriter file" << endl;
                               fclose(fout);
                               fout = nullptr;
                           } );

    bool run = true;
    int nbytes;
    buffer.reset( new matrix::GenericBuffer() );
    // buffer->resize(blocksize);
    
    while (run)
    {
        try
        {
            data_sink.get(*buffer);
            nbytes = fwrite(buffer->data(), buffer->size(), 1, fout);
            if (nbytes != buffer->size())
            {
                cout << __PRETTY_FUNCTION__ << " wrote " << nbytes
                << " needed to write " << buffer->size() << endl;
            }
        }
        catch (MatrixException e)
        {
            cout << __PRETTY_FUNCTION__ << e.what() << endl;
            stop();
        }
        _run.get_value(run);
    }
}


bool FileDataSink::connect()
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

    try
    {
        if (!connect_sink(data_sink, "data_sink"))
        {
            cout << __PRETTY_FUNCTION__ << " sink failed to connect to input data source" << endl;
            stop();
            return false;
        }
    }
    catch (MatrixException e)
    {
        cout << __PRETTY_FUNCTION__ << " exception in connect " << e.what() << endl;
        stop();
        return false;
    }
    return true;
}

bool FileDataSink::disconnect()
{
    data_sink.disconnect();
    return true;
}

bool FileDataSink::_stop()
{
    keymaster->put(my_full_instance_name + ".command", "stop");
    cout << __PRETTY_FUNCTION__ << " _stop()" << endl;
    return true;
}

bool FileDataSink::_do_start()
{
    if (!connect())
    {
        return false;
    }

    if (!_write_thread.running())
    {
        cout << "FileDataSink::_do_start(): starting thread." << endl;
        _run.set_value(true);
        _write_thread.start("FileReader");
    }

    _run.set_value(true);
    bool rval = _write_thread_started.wait(true, 5000000);

    if (rval)
    {
        cout << "FileDataSink started." << endl;
    }
    else
    {
        cout << "FileDataSink failed to start!" << endl;
        _run.set_value(false);
        if (_write_thread.running())
        {
            _write_thread.stop();
        }
    }

    return rval;
}

bool FileDataSink::_do_stop()
{
    if (_write_thread.running())
    {
        cout << "FileDataSink::_do_stop(): Killing thread." << endl;
        _run.signal(false);
        _write_thread.join();
        cout << "FileDataSink dead." << endl;
    }

    _write_thread_started.set_value(false);
    _run.set_value(false);
    disconnect();
    return true;
}

