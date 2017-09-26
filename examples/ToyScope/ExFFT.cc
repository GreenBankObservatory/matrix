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


#include "ExFFT.h"
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
ExFFT::factory(string name, string km_url)
{
    return new ExFFT(name, km_url);
}

ExFFT::ExFFT(string name, string km_url) :
    Component(name, km_url),
    input_signal_sink(km_url),
    output_signal_source(km_url, my_instance_name, "fft_out"),
    poll_thread(this, &ExFFT::poll),
    poll_thread_started(false),
    operation(NONE),
    N(128)
{
    string looking_for;
    yaml_result yn;
    
    looking_for = my_full_instance_name + ".operation";
    if(keymaster->get(looking_for, yn))
    {
        parse_operation(yn.node.as<string>());
        string key = my_full_instance_name + ".operation";
        cout << "subscribing to " << key << endl;
        keymaster->subscribe(key, new KeymasterMemberCB<ExFFT>(this,
                                  &ExFFT::operation_changed));        
    }
    looking_for = my_full_instance_name + ".N";
    if(keymaster->get(looking_for, yn))
    {
        N = yn.node.as<int>();
        string key = my_full_instance_name + ".N";
        cout << "subscribing to " << key << endl;
        keymaster->subscribe(key, new KeymasterMemberCB<ExFFT>(this,
                                  &ExFFT::N_changed));        
    }
    
}

void ExFFT::operation_changed(string path, YAML::Node new_op)
{
    parse_operation(new_op.as<string>());
}

void ExFFT::parse_operation(string op)
{
    if (op == "none" || op == "NONE")
        operation = NONE;
    else if (op == "square" || op == "SQUARE")
        operation = SQUARE;
    else
        cout << "Unrecognized operation " << op << endl;
}

void ExFFT::N_changed(string path, YAML::Node new_N)
{
    N = new_N.as<int>();
}

/// Disconnect and release resources.
ExFFT::~ExFFT()
{
    disconnect();
}

/// Reads 'decimation_factor samples, averages them, and outputs the average
/// to a linux fifo. An external application reads and displays the result.
/// Note: No window is applied so output may have artifacts.
void ExFFT::poll()
{
    int ctr = 0;
    poll_thread_started.signal(true);
    double data;
    int i;
    fftw_complex *in, *out;
    fftw_plan p;
    
    in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N);
    out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N);

    p = fftw_plan_dft_1d(N, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
      
    while (1)
    {
        // spin until N values have been read
        for (i=0; i<N; ++i)
        {
            double datain;
            input_signal_sink.get(datain);
            // should probably generate both I/Q values ...
            in[i][0] = datain;
            in[i][1] = 0.0;
        }
        // perform the complex-complex FFT
        fftw_execute(p);
        for (i=0; i<N; ++i)
        {
            // calculate the power:
            double dataout = out[i][0]*out[i][0] + out[i][1]*out[i][1];
            output_signal_source.publish(dataout);
        }
        printf("fftcycle\n");            
    }
    
    fftw_destroy_plan(p);
    fftw_free(in);
    fftw_free(out);
}

bool
ExFFT::connect()
{
    // find the src component and sourcename for our sink in this mode
    return connect_sink(input_signal_sink, "input_data");
}
bool
ExFFT::disconnect()
{
    input_signal_sink.disconnect();
    return true;
}

bool
ExFFT::_do_start()
{
    // find the src component and sourcename for our sink in this mode
    connect();
    if (!poll_thread.running())
    {
        poll_thread.start();
    }
    poll_thread_started.wait(true); // should wait with timeout...
    return true;
}

bool
ExFFT::_do_stop()
{
    disconnect();
    
    if (poll_thread.running())
    {
        poll_thread.stop();
    }
    return true;
}

