
#ifndef make_path_h
#define make_path_h
#include <string>
#include "matrix/Time.h"

namespace matrix
{
    /// C++ utility to recursively create a directory path
    bool make_path(std::string path);

    /// create a M&C style log name e.g '2015_12_31_11:59:59' (no extension)
    void generate_log_filename(Time::Time_t time, std::string &fname);
};
#endif

