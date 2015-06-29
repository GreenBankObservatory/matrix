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




#ifndef ExSignalGenerator_h
#define ExSignalGenerator_h

#include "Time.h"
#include "Component.h"
#include "DataInterface.h"
#include "DataSource.h"
#include "DataSink.h"

/// An example of a really silly signal generator which can generate
/// a tone (sine wave), noise, or a constant amplitude signal.
///
/// Component Specific Keywords:
/// *  rate - an integer controlling the number of samples output per second
/// *  waveform - one of "tone", "noise", "DC"
/// *  amplitude - multiplier on generated signal
/// Note: the above keywords may be changed while running
///
/// Data Sinks (Inputs) - None
/// Data Sources (Outputs):
/// *  "wavedata" - a single double value
///
class ExSignalGenerator : public Component
{
public:
  
    static Component *factory(std::string, std::string);
    virtual ~ExSignalGenerator();
    
protected:    
    ExSignalGenerator(std::string name, std::string km_url);

    /// Run-state loop 
    void poll();
    
    // override various base class methods
    virtual bool _do_start();
    virtual bool _do_stop();        
    
    bool connect();
    bool disconnect();
    void rate_changed(std::string, YAML::Node new_rate);
    void waveform_changed(std::string, YAML::Node new_waveform);
    void amplitude_changed(std::string, YAML::Node new_amplitude);
    void frequency_changed(std::string, YAML::Node new_frequency);
protected:
    matrix::DataSource<double>     output_signal_source;    
  
    Thread<ExSignalGenerator>       poll_thread;
    TCondition<bool>                poll_thread_started;
    double amplitude;
    double frequency;

    int waveform_type;
    int rate_factor;
    
    enum WaveType {TONE, NOISE, DC};
     
    
};

#endif


