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



#ifndef ExProcessor_h
#define ExProcessor_h

#include "matrix/Time.h"
#include "matrix/Component.h"
#include "matrix/DataInterface.h"
#include "matrix/DataSource.h"
#include "matrix/DataSink.h"

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

class ExProcessor : public Component
{
public:
  
    static Component *factory(std::string, std::string);
    virtual ~ExProcessor();
    
protected:    
    ExProcessor(std::string name, std::string km_url);

    /// Run-state loop 
    void poll();
    
    // override various base class methods
    virtual bool _do_start();
    virtual bool _do_stop();        
    
    bool connect();
    bool disconnect();

    void operation_changed(std::string, YAML::Node new_operation);
    void parse_operation(std::string);
    
protected:
    matrix::DataSink<double, matrix::select_only>     input_signal_sink;    
    matrix::DataSource<double>     output_signal_source;
  
    Thread<ExProcessor>         poll_thread;
    TCondition<bool>            poll_thread_started;
    int operation;
    enum Operation { NONE, SQUARE }; 
    
};

#endif


