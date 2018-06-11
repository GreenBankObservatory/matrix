
#ifndef FITSLogger_h
#define FITSLogger_h

#include <string>
#include <vector>
#include <cstdio>
#include <stdarg.h>
#include "matrix/Mutex.h"
#include "matrix/ThreadLock.h"
#include "matrix/DataInterface.h"
#include <fitsio.h>

/// A general log data writer which works with the matrix GenericBuffer.
class FITSLogger
{
public:

    FITSLogger(YAML::Node ddyaml, std::string header, int debuglevel=0);

    /// cleaup and close file
    virtual ~FITSLogger();

    /// close (if open) current file and open a new one.
    bool set_file(std::string);

    /// open a time-named log file:
    bool open_log();

    /// safe check on status of file
    bool is_log_open();

    /// Set the directory for log files. *Does* not impact currently opened file.
    bool set_directory(std::string);

    /// creates the Binary table header
    bool create_header();

    /// writes the data to the log file in the calling context (possibly blocking)
    /// Should only be used from soft-rt context.
    bool log_data(matrix::GenericBuffer &);

    /// closes the current file.
    void close();

    /// returns the specified size of the data. The GenericBuffer should be
    /// resized to this size.
    size_t log_datasize() { return ddesc.size(); }



protected:
    std::string directory_name;
    std::string file_name;
    std::string header;
    size_t log_size;
    matrix::data_description ddesc;

    matrix::Mutex mtx;
    int status;
    int last_reported_status;
    fitsfile *fout;
    int cur_row;

};

#endif

