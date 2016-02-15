
#include "FITSLogger.h"
#include <vector>
#include <cstdio>
#include <sstream>
#include <iostream>
#include <sys/stat.h>
#include "make_path.h"
#include <string.h>
#include "Time.h"
#include "DataInterface.h"
#include "DataSink.h"

using namespace std;
using namespace matrix;

static int debug = 0;
#define dbprintf if(debug) printf

FITSLogger::FITSLogger(YAML::Node ystr, string hdr, int debuglevel) :
    ddesc(ystr),
    mtx(),
    status(0),
    last_reported_status(0),
    fout(nullptr)
{
    debug = debuglevel;
    (void)ddesc.size();
}

FITSLogger::~FITSLogger()
{
    if (fout)
    {
        close();
    }
}

bool FITSLogger::set_directory(string dir)
{
    directory_name = dir;
    // Check and make sure dir is really a directory
    return make_path(directory_name);
}

bool FITSLogger::set_file(string fname)
{
    file_name = fname;
    ThreadLock<Mutex> lck(mtx);
    lck.lock();
    close();

    string fullname = directory_name + "/" + file_name;

    // create the file
    status = 0;
    fits_create_file(&fout, fullname.c_str(), &status);
    if (status != 0)
    {
        cout << "Problem creating file:" << fullname << endl;
        cout << "status " << status << endl;
        cout << "exiting" << endl;
        return false;
    }

    return create_header();
}

static string get_type_code(data_description::types t, size_t n = 1)
{
    string tstr;
    switch (t)
    {
        case data_description::DOUBLE:
            tstr = to_string(n) + "D";
            break;
        case data_description::FLOAT:
            tstr = to_string(n) + "E";
            break;
        case data_description::INT64_T:
        case data_description::LONG:
            tstr = to_string(n) + "K";
            break;
        case data_description::UINT64_T:
        case data_description::UNSIGNED_LONG:
            tstr = to_string(n) + "K";
            break;
        case data_description::INT:
        case data_description::INT32_T:
            tstr = to_string(n) + "J";
            break;
        case data_description::UINT32_T:
        case data_description::UNSIGNED_INT:
            tstr = to_string(n) + "V";
            break;
;
        case data_description::INT16_T:
        case data_description::SHORT:
            tstr = to_string(n) + "I";
            break;
        case data_description::UNSIGNED_SHORT:
        case data_description::UINT16_T:
            tstr = to_string(n) + "U";
            break;
        case data_description::INT8_T:
        case data_description::CHAR:
            tstr = to_string(n) + "S";
            break;
        case data_description::UNSIGNED_CHAR:
        case data_description::UINT8_T:
            tstr = to_string(n) + "B";
            break;
        default:
            printf("not supported %d\n", t);
        break;
    }
    return tstr;
}

bool FITSLogger::create_header()
{
    char keyname[10];
    char comment[64];
    char value[64];
    bool rtn = true;

    // create a primary header
    fits_create_img(fout, 8, 0, 0, &status);

    // Write a few comments. Should probably write an M&C sampler header ...
    strcpy(keyname, "INSTRUME");
    strcpy(comment, "device or program of origin");
    strcpy(value, "GBT");
    fits_update_key_str(fout, keyname, value, comment, &status);
    strcpy(keyname, "ORIGIN");
    strcpy(comment, "subsystem");
    strcpy(value, "GBTCCU");
    fits_update_key_str(fout, keyname, value, comment, &status);

    // now create the binary table
    int ncols = ddesc.fields.size();
    int extnum = 2;
    long nrows = 0;
    char **tnames, **tform, **tunit;

    tnames = new char *[ncols];
    tform = new char *[ncols];
    tunit = new char *[ncols];
    for (int i = 0; i < ncols; ++i)
    {
        tnames[i] = new char[80];
        tform[i] = new char[80];
        tunit[i] = new char[80];
        memset(tnames[i], 0, 80);
        memset(tform[i], 0, 80);
        memset(tunit[i], 0, 80);
    }
    int fits_cols = 0;
    for (auto dd = ddesc.fields.begin(); dd != ddesc.fields.end(); ++dd)
    {
        // omit any skipped fields
        if (dd->skip)
            continue;
        strcpy(tnames[fits_cols], dd->name.c_str());
        strcpy(tform[fits_cols], get_type_code(dd->type).c_str());
        strcpy(tunit[fits_cols], "none");
        ++fits_cols;
    }

    if (status == 0)
    {
        fits_create_tbl(fout, BINARY_TBL, nrows, fits_cols, tnames, tform, tunit, "DATA", &status);
    }
    else
    {
        printf("could not create img & tbl error=%d\n", status);
        rtn = false;
    }
    fits_flush_file(fout, &status);
    // clean up - I hate FITS ...
    for (int idx = 0; idx < ncols; ++idx)
    {
        delete[] tnames[idx];
        delete[] tform[idx];
        delete[] tunit[idx];
    }
    delete[] tnames;
    delete[] tform;
    delete[] tunit;

    cur_row = 0;

    return rtn;
}


bool FITSLogger::is_log_open()
{
    ThreadLock<Mutex> lck(mtx);
    lck.lock();
    return fout != nullptr;
}
bool FITSLogger::open_log()
{
    string name;
    Time::Time_t now = Time::getUTC();
    generate_log_filename(now, name);
    return set_file(name + ".fits");
}

void FITSLogger::close()
{
    if (fout)
    {
        ThreadLock<Mutex> lck(mtx);
        lck.lock();
        fits_close_file(fout, &status);
        fout = nullptr;
        lck.unlock();
    }
}


/// Log a row of data
bool FITSLogger::log_data(GenericBuffer &data)
{
    // if the file isn't open, silently ignore the data.
    if (fout == nullptr)
    {
        return false;
    }

    int columnNum = 1;

    fits_insert_rows(fout, cur_row, 1, &status);
    ++cur_row;

    for (auto z=ddesc.fields.begin(); z!=ddesc.fields.end(); )
    {
        // skip over unused fields
        if (z->skip)
        {
            ++z;
            continue;
        }
        switch (z->type)
        {
            case data_description::DOUBLE:
            {
                double d = get_data_buffer_value<double>(data.data(), z->offset);
                dbprintf("DBL:%f", d);
                fits_write_col_dbl(fout, columnNum, (LONGLONG)cur_row, 1LL, 1LL, &d, &status);
                break;
            }
            case data_description::FLOAT:
            {
                float d = get_data_buffer_value<float>(data.data(), z->offset);
                dbprintf("%f", d);
                fits_write_col_flt(fout, columnNum, cur_row, 1, 1, &d, &status);
                break;
            }
            case data_description::INT64_T:
            case data_description::LONG:
            {
                int64_t d = get_data_buffer_value<int64_t>(data.data(), z->offset);
                dbprintf("%lld", d);
                fits_write_col_lnglng(fout, columnNum, cur_row, 1, 1, (LONGLONG*)&d, &status);
                break;
            }
            case data_description::INT32_T:
            case data_description::INT:
            {
                int32_t d = get_data_buffer_value<int32_t>(data.data(), z->offset);
                dbprintf("%d", d);
                fits_write_col_int(fout, columnNum, cur_row, 1, 1, &d, &status);
                break;
            }
            case data_description::INT16_T:
            case data_description::SHORT:
            {
                int16_t d = get_data_buffer_value<int16_t>(data.data(), z->offset);
                dbprintf("%d", d);
                fits_write_col_sht(fout, columnNum, cur_row, 1, 1, (short *)&d, &status);
                break;
            }
            case data_description::INT8_T:
            case data_description::CHAR:
            {
                int8_t d = get_data_buffer_value<int8_t>(data.data(), z->offset);
                dbprintf("%c", d);
                fits_write_col_byt(fout, columnNum, cur_row, 1, 1, (unsigned char *)&d, &status);
                break;
            }
            case data_description::UINT64_T:
            case data_description::UNSIGNED_LONG:
            {
                uint64_t d = get_data_buffer_value<uint64_t>(data.data(), z->offset);
                dbprintf("%llu", d);
                fits_write_col_lnglng(fout, columnNum, cur_row, 1, 1, (LONGLONG*)&d, &status);
                break;
            }
            case data_description::UINT32_T:
            case data_description::UNSIGNED_INT:
            {
                uint32_t d = get_data_buffer_value<uint32_t>(data.data(), z->offset);
                dbprintf("%u", d);
                fits_write_col_uint(fout, columnNum, cur_row, 1, 1, (unsigned int *)&d, &status);
                break;
            }
            case data_description::UINT16_T:
            case data_description::UNSIGNED_SHORT:
            {
                uint16_t d = get_data_buffer_value<uint16_t>(data.data(), z->offset);
                dbprintf("%u", d);
                fits_write_col_usht(fout, columnNum, cur_row, 1, 1, (unsigned short *)&d, &status);
                break;
            }
            case data_description::UINT8_T:
            case data_description::UNSIGNED_CHAR:
            {
                uint8_t d = get_data_buffer_value<uint8_t>(data.data(), z->offset);
                dbprintf("%u", d);
                fits_write_col_byt(fout, columnNum, cur_row, 1, 1, (unsigned char *)&d, &status);
                break;
            }
            case data_description::LONG_DOUBLE:
            {
                // I'm not sure FITS knows this type?
                printf("long double not supported\n");
                break;
            }

            default:
                printf("type %d not supported\n", z->type);
                break;
        }

        if (status != 0 && status != last_reported_status)
        {
            printf("Error %d\n", status);
            last_reported_status = status;
        }
        ++z;

        // if there are more entries, otherwise flush
        if (z!=ddesc.fields.end())
        {
            dbprintf(", ");
            ++columnNum;
        }
        else
        {
            dbprintf("\n");
            fits_flush_file(fout, &status);
        }
    }
}



