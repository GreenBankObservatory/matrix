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



#ifndef ExFFT_h
#define ExFFT_h

#include "matrix/Time.h"
#include "matrix/Component.h"
#include "matrix/DataInterface.h"
#include "matrix/DataSource.h"
#include "matrix/DataSink.h"
#include <fftw3.h>

/// A simple 'processor' component which has one sink input and one source
/// output. Two types of operations can be specified: 
/// 'none'   - data is copied as-is from input to output or 
/// 'square' - input data is squared before output
///
/// Component Specific Keywords:
/// *  operation - a string specifying the operation to perform, as noted above.
/// Note: the above keyword may be changed while running
///
/// Data Sinks (Inputs) :
/// *  'input_data' - format is one type double element
/// Data Sources (Outputs):
/// *  'processed_data' - a copy of the input data with the specified operation
/// applied (e.g. squaring).

struct FFTIn
{
    double in[256][2];
};

class ExFFT : public matrix::Component
{
public:
  
    static matrix::Component *factory(std::string, std::string);
    virtual ~ExFFT();
    
protected:    
    ExFFT(std::string name, std::string km_url);

    /// Run-state loop 
    void poll();
    
    // override various base class methods
    virtual bool _do_start();
    virtual bool _do_stop();        
    
    bool connect();
    bool disconnect();

    void operation_changed(std::string, YAML::Node new_operation);
    void N_changed(std::string path, YAML::Node new_op);
    void parse_operation(std::string);
    
protected:
    matrix::DataSink<double, matrix::select_only>     input_signal_sink;    
    matrix::DataSource<double>     output_signal_source;

    matrix::Thread<ExFFT>         poll_thread;
    matrix::TCondition<bool>            poll_thread_started;
    int operation;
    int N;
    enum Operation { NONE, SQUARE }; 
    
};

#endif


