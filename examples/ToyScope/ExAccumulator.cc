// ======================================================================
// Copyright (C) 2015 Associated Universities, Inc. Washington DC, USA.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
// Correspondence concerning GBT software should be addressed as follows:
//  GBT Operations
//  National Radio Astronomy Observatory
//  P. O. Box 2
//  Green Bank, WV 24944-0002 USA


#include "ExAccumulator.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <cstdio>
#include <exception>
#include <cmath>
#include "yaml_util.h"

using namespace std;
using namespace Time;
using namespace mxutils;

static void throw_value_error(string key, string msg)
{
    cerr << msg << " must be provided for the "
         << key << " keyword" << endl;
    cerr << "NOTE: this is considered a fatal error. Check Configuration"
         << endl;
    KeymasterException q("Key value invalid");
    throw q;            
}

Component *
ExAccumulator::factory(string name, string km_url)
{
    return new ExAccumulator(name, km_url);
}

ExAccumulator::ExAccumulator(string name, string km_url) :
    Component(name, km_url),
    input_signal_sink(km_url),
    fout(0),
    poll_thread(this, &ExAccumulator::poll),
    poll_thread_started(false),
    decimate_factor(1)
{
    string looking_for;
    yaml_result yn;
    
    looking_for = my_full_instance_name + ".decimate";
    if(keymaster->get(looking_for, yn))
    {
        decimate_factor = yn.node.as<int>();
        if (!decimate_factor > 0)
            throw_value_error(looking_for, "decimate keyword must be greater than zero");
    }
    else
    {
        KeymasterException q(looking_for + yn.err);
        throw q;
    }
    string prefix="components.";
    string key = my_full_instance_name + ".decimate";
    keymaster->subscribe(key, new KeymasterMemberCB<ExAccumulator>(this,
                                  &ExAccumulator::decimate_changed));    
}

/// Disconnect and release resources.
ExAccumulator::~ExAccumulator()
{
}

/// Reads 'decimation_factor samples, averages them, and outputs the average
/// to a linux fifo. An external application reads and displays the result.
void ExAccumulator::poll()
{
    int ctr = 0;
    poll_thread_started.signal(true);
    double avg, sum, sample;
    int i;
    
    while (1)
    {
        sum = 0.0;
        for (i=0; i<decimate_factor; ++i)
        {
            input_signal_sink.get(sample);
            sum += sample;
        }
        avg = sum/decimate_factor;
        
        if (fout)
        {
            fprintf(fout, "%f\n", avg);
            fflush(fout);
            printf("AC: %f\n", avg);
        }
        else
        {
            printf("fifo not working\n");
        }

    }
}

void
ExAccumulator::decimate_changed(string path, YAML::Node new_decimate)
{
    decimate_factor = new_decimate.as<int>();
    cout << "decimate now " << new_decimate << endl;
}

bool
ExAccumulator::connect()
{
    // find the src component and sourcename for our sink in this mode
    ConnectionKey q(current_mode, my_instance_name, "input_data");
    if (find_data_connection(q))
    {
        input_signal_sink.connect(get<0>(q), get<1>(q), get<2>(q));
    }
    if (fout == 0)
    {
        fout = fopen("/tmp/data", "w+");
        if (!fout)
        {
            printf("Could not open fifo - dont expect anything to work!\n");
            return false;
        }
    }
    return true;
}
bool
ExAccumulator::disconnect()
{
    input_signal_sink.disconnect();
    if (fout)
    {
        fclose(fout);
        fout = 0;
    }
    return true;
}

bool
ExAccumulator::_do_start()
{
    connect();
    if (!poll_thread.running())
    {
        poll_thread.start();
    }
    poll_thread_started.wait(true); // should really wait with timeout...
    return true;
}

bool
ExAccumulator::_do_stop()
{
    if (poll_thread.running())
    {
        poll_thread.stop();
    }
    disconnect();    
    return true;
}

