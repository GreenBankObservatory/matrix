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


#include "ExSignalGenerator.h"
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
ExSignalGenerator::factory(string name, string km_url)
{
    return new ExSignalGenerator(name, km_url);
}

ExSignalGenerator::ExSignalGenerator(string name, string km_url) :
    Component(name, km_url),
    output_signal_source(km_url, my_instance_name,  "wavedata"),
    poll_thread(this, &ExSignalGenerator::poll),
    poll_thread_started(false),
    amplitude(1.0),
    waveform_type(TONE),
    rate_factor(1)
{
    string looking_for;
    yaml_result yn;

    looking_for = my_full_instance_name + ".rate";
    if(keymaster->get(looking_for, yn))
    {
        rate_factor = yn.node.as<int>();
        if (!rate_factor > 0)
            throw_value_error(looking_for, "rate keyword must be greater than zero");
    }
    else
    {
        KeymasterException q(looking_for + yn.err);
        throw q;
    }
    string key = my_full_instance_name + ".rate";
    cout << "subscribing to " << key << endl;
    keymaster->subscribe(key, new KeymasterMemberCB<ExSignalGenerator>(this,
                                  &ExSignalGenerator::rate_changed));
    key = my_full_instance_name + ".waveform";
    cout << "subscribing to " << key << endl;
    keymaster->subscribe(key, new KeymasterMemberCB<ExSignalGenerator>(this,
                                  &ExSignalGenerator::waveform_changed));
    key = my_full_instance_name + ".amplitude";
    cout << "subscribing to " << key << endl;
    keymaster->subscribe(key, new KeymasterMemberCB<ExSignalGenerator>(this,
                                  &ExSignalGenerator::amplitude_changed));                         
}

/// Disconnect and release resources.
ExSignalGenerator::~ExSignalGenerator()
{
}

/// Reads 'decimation_factor samples, averages them, and outputs the average
/// to a linux fifo. An external application reads and displays the result.
void ExSignalGenerator::poll()
{
    int ctr = 0;
    poll_thread_started.signal(true);
    double avg, sum, sample;
    int i;
    double n;
    n=0;
        while (1)
    {
        Time_t delay = 1000000000/static_cast<Time_t>(rate_factor);
        Time::thread_delay(delay);
        switch (waveform_type)
        {
            case TONE:
                sample = amplitude * cos(n/180.0 * M_PI);
                n = n+5.0;
            break;
            case NOISE:
                sample = amplitude * static_cast<double>(rand())/RAND_MAX;
            break;
            case DC:
                sample = amplitude;
            break;
            default:
                printf("unknown waveform?\n");
            break;
        }
        output_signal_source.publish(sample);        
        printf("SG: %f\n", sample);  
    }
}

void ExSignalGenerator::rate_changed(string path, YAML::Node new_rate)
{
    cout << "rate now" << new_rate << endl;
    rate_factor = new_rate.as<int>();
}

void ExSignalGenerator::waveform_changed(string path, YAML::Node new_waveform)
{
    cout << "waveform now" << new_waveform << endl;
    if (new_waveform.as<string>() == "tone")
        waveform_type = TONE;
    else if (new_waveform.as<string>() == "noise")
        waveform_type = NOISE;
    else if (new_waveform.as<string>() == "DC")
        waveform_type = DC;
    else
        cout << "dont know waveform type " << new_waveform << endl;
}

void ExSignalGenerator::amplitude_changed(string path, YAML::Node new_amplitude)
{
    cout << "amplitude now" << new_amplitude << endl;
    amplitude = new_amplitude.as<double>();
}

bool
ExSignalGenerator::connect()
{
    // Source only Components really don't need connect/disconnect
    return true;
}
bool
ExSignalGenerator::disconnect()
{
    // Source only Components really don't need connect/disconnect
    return true;
}

bool
ExSignalGenerator::_do_start()
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
ExSignalGenerator::_do_stop()
{
    if (poll_thread.running())
    {
        poll_thread.stop();
    }
    disconnect();    
    return true;
}

