#include <iostream>
#include "DataInterface.h"
#include "DataSink.h"
#include <string>
#include <vector>
#include <memory>
#include "FITSLogger.h"

using namespace std;
using namespace matrix;

const char helpstr[] =
"Slogger, a DataSink to fits logger program.                                                   \n"
"usage: slogger -str stream_alias [ -debug ]  [ -url keymaster_url ] [ -ldir path ]            \n"
"The environment variable MATRIXLOGDIR can be used to specify where log files                  \n"
"will be written. Alternatively this can be specifies using the -ldir option.                  \n"
"                                                                                              \n"
"slogger relies upon a two sections in the keymaster which ties additional                     \n"
"data stream information to an user-friendly alias.                                            \n"
"                                                                                              \n"
"Example YAML:                                                                                 \n"
"# The streams section is a list of human readable aliases for a specific source.              \n"
"# Each entry lists the data source component, the source name, and the data description key   \n"
"# into the stream_descriptions table.                                                         \n"
"streams:                                                                                      \n"
"    az_encoder: [src_component1, src_name1, src_ddesc_name]                                   \n"
"    el_encoder: [src_component2, src_name2, src_ddesc_name]                                   \n"
"                                                                                              \n"
"# The stream_descriptions table lists descriptions of types                                   \n"
"# of a source-sink stream of interest.                                                        \n"
"stream_descriptions:                                                                          \n"
"    src_ddesc_name:                                                                           \n"
"        fields:                                                                               \n"
"            0: [time, double, 1]                                                              \n"
"            1: [position, double, 1]                                                          \n"
"            2: [position_error, double, 1]                                                    \n"
"            3: [commanded_rate, double, 1]                                                    \n"
"                                                                                              \n"
"                                                                                              \n"
"\n";


int main(int argc, char **argv)
{


    string log_dir;
    string compname;
    string srcname;
    string axis_prefix;
    string data_description;
    string data_header;
    string arg;

    // defaults
    int debuglevel = 0;
    size_t max_rows_per_file = 256*1024; // 262144 rows default
    string keymaster_url = "tcp://localhost:42000";

    const char *log_base = getenv("MATRIXLOGDIR");

    if (log_base)
    {
        log_dir = string(log_base);
    }

    // usage print
    if (argc < 2)
    {
        cout << "usage: slogger -str stream_alias [-ldir path] [-maxrows nrows] [-debug] [-help]" << endl;
        exit(-1);
    }

    string stream_alias;
    for (int i=1; i<argc; ++i)
    {
        arg = argv[i];

        if (arg == "-str")
        {
            // a path to the ddesc
            ++i;
            arg = argv[i];
            stream_alias = string("streams.") + arg;
        }
        else if (arg == "-url")
        {
            ++i;
            keymaster_url = argv[i];
        }
        else if (arg == "-ldir")
        {
            ++i;
            log_dir = argv[i];
        }
        else if (arg == "-debug")
        {
            debuglevel = 1;
        }
        else if (arg == "-help")
        {
            cout << helpstr << endl;
            return -1;
        }
        else
        {
            cout << "Unrecognized option:" << arg << endl;
            cout << helpstr << endl;
            return -1;
        }
    }

    if (log_dir.size() < 1)
    {
        cout << "logging path not set - using /tmp" << endl;
        log_dir = "/tmp";
    }

    YAML::Node dd_node;
    string stream_dd_path;
    Keymaster keymaster(keymaster_url);
    DataSink<GenericBuffer> sink(keymaster_url);

    try
    {
        dd_node = keymaster.get(stream_alias);
    }
    catch(KeymasterException e)
    {
        cout << "Error getting key: " << stream_alias << endl;
        cout << e.what() << endl;
        return -1;
    }

    if (dd_node.size() >= 3)
    {
        compname = dd_node[0].as<string>();
        srcname = dd_node[1].as<string>();
        stream_dd_path = string("stream_descriptions.") + dd_node[2].as<string>();
    }
    else
    {
        cout << argv[0] << " Unexpected stream_description format| " << dd_node << endl;
        return -1;
    }
    YAML::Node stream_dd;
    try
    {
        stream_dd = keymaster.get(stream_dd_path + ".fields");
    }
    catch(KeymasterException e)
    {
        cout << "Error getting key:" << stream_dd_path + ".fields" << endl;
        cout << e.what();
        return -1;
    }
    FITSLogger log(stream_dd, data_header, debuglevel);

    log.set_directory(log_dir + "/");
    if (!log.open_log())
    {
        cout << "Error opening log file: "
             <<  strerror(errno) << endl;
        return(-1);
    }

    sink.connect(compname, srcname, "");

    if (!sink.connected())
    {
        cout << "Sink could not connect to component/source:"
             << compname << "/" << srcname << endl;
        return -1;
    }

    int nrows = 0;

    GenericBuffer gbuffer;

    gbuffer.resize(log.log_datasize());
    while (1)
    {
        sink.get(gbuffer);
        log.log_data(gbuffer);

        if (++nrows > max_rows_per_file)
        {
            printf("opening new file\n");
            log.close();
            if (!log.open_log())
            {
                return(-1);
            }
            nrows=0;
        }
    }

    return 0;
}
