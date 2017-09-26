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


#include "ExProcessor.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <cstdio>
#include <exception>
#include <cmath>
#include "matrix/yaml_util.h"

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

Component *
ExProcessor::factory(string name, string km_url)
{
    return new ExProcessor(name, km_url);
}

ExProcessor::ExProcessor(string name, string km_url) :
    Component(name, km_url),
    input_signal_sink(km_url),
    output_signal_source(km_url, my_instance_name, "processed_data"),
    poll_thread(this, &ExProcessor::poll),
    poll_thread_started(false),
    operation(NONE)
{
    string looking_for;
    yaml_result yn;
    
    looking_for = my_full_instance_name + ".operation";
    if(keymaster->get(looking_for, yn))
    {
        parse_operation(yn.node.as<string>());
    }
    else
    {
        KeymasterException q(looking_for + yn.err);
        throw q;
    }
    string key = my_full_instance_name + ".operation";
    cout << "subscribing to " << key << endl;
    keymaster->subscribe(key, new KeymasterMemberCB<ExProcessor>(this,
                                  &ExProcessor::operation_changed));
    
}

void ExProcessor::operation_changed(string path, YAML::Node new_op)
{
    parse_operation(new_op.as<string>());
}

void ExProcessor::parse_operation(string op)
{
    if (op == "none" || op == "NONE")
        operation = NONE;
    else if (op == "square" || op == "SQUARE")
        operation = SQUARE;
    else
        cout << "Unrecognized operation " << op << endl;
}

/// Disconnect and release resources.
ExProcessor::~ExProcessor()
{
    disconnect();
}

/// Reads 'decimation_factor samples, averages them, and outputs the average
/// to a linux fifo. An external application reads and displays the result.
void ExProcessor::poll()
{
    int ctr = 0;
    poll_thread_started.signal(true);
    double data;
    
    while (1)
    {
        switch (operation)
        {
            case NONE:
                input_signal_sink.get(data);
                output_signal_source.publish(data);
            break;
            case SQUARE:
                input_signal_sink.get(data);
                data = data*data;
                output_signal_source.publish(data);
            break;
        }
    }
}

bool
ExProcessor::connect()
{
    // find the src component and sourcename for our sink in this mode
    return connect_sink(input_signal_sink, "input_data");
}
bool
ExProcessor::disconnect()
{
    input_signal_sink.disconnect();
    return true;
}

bool
ExProcessor::_do_start()
{
    // find the src component and sourcename for our sink in this mode
    ConnectionKey q(current_mode, my_instance_name, "input_data");
    if (find_data_connection(q))
    {
        input_signal_sink.connect(get<0>(q), get<1>(q), get<2>(q));
    }
    
    if (!poll_thread.running())
    {
        poll_thread.start();
    }
    poll_thread_started.wait(true); // should wait with timeout...
    return true;
}

bool
ExProcessor::_do_stop()
{
    input_signal_sink.disconnect();
    
    if (poll_thread.running())
    {
        poll_thread.stop();
    }
    return true;
}

