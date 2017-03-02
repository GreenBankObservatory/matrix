/*******************************************************************
 *  FileDataSink .h - Declares the FileDataSink component
 *  class.
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

#ifndef FileDataSink_h
#define FileDataSink_h

#include "matrix/Component.h"
#include "matrix/DataInterface.h"
#include "matrix/DataSource.h"
#include "matrix/DataSink.h"

/**
 * \class FileDataSink
 *
 * This component writes data to a file, with a specified blocking
 * factor (i.e how much data per publish). There is no limit on file
 * size.
 *
 */

class FileDataSink : public Component
{
public:

    static Component *factory(std::string, std::string);
    virtual ~FileDataSink();

protected:
    FileDataSink(std::string name, std::string km_url);

    // Run file reader
    void _writer_thread();

    // override various base class methods
    virtual bool _do_start();
    virtual bool _do_stop();
    virtual bool _stop();

    bool connect();
    bool disconnect();

    matrix::DataSink<matrix::GenericBuffer> data_sink;

    Thread<FileDataSink> _write_thread;
    TCondition<bool> _write_thread_started;
    TCondition<bool> _run;

    std::unique_ptr<matrix::GenericBuffer> buffer;

    size_t blocksize;
    std::string filename;

};

#endif


