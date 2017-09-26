
#include <cstdio>
#include <cerrno>
#include <sstream>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <array>
#include <string>
#include <unistd.h>
#include "matrix/Time.h"

using namespace std;


#define DEFAULT_MODE S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH
namespace matrix
{
    bool make_path(string pathname)
    {
        // fastpath - path exists
        struct stat stat_result;
        if (stat(pathname.c_str(), &stat_result) == 0)
        {
            return S_ISDIR(stat_result.st_mode);
        }
        // quick corner error case
        if (pathname.size() == 0)
        {
            return false;
        }
        // Ok, we are not so lucky, the directory isn't already there.
        istringstream path(pathname);
        string fs;

        // handle the prefix . or / or no prefix
        array<char, 256> dir_component;
        path.getline(dir_component.data(), dir_component.size(), '/');

        string prefix(dir_component.data());

        // a full path - remember slash doesn't end up in 'a'
        if (prefix == "")
        {
            fs = "";
        }
            // the ./somedir case. mkdir doesn't like this, so
            // we prepend the cwd instead.
        else if (prefix == ".")
        {
            //
            char cwd[256];
            getcwd(cwd, sizeof(cwd));
            fs = cwd;
        }
        else if (prefix.size() > 0)
        {
            char cwd[256];
            getcwd(cwd, sizeof(cwd));
            fs = string(cwd) + "/" + prefix;
            cout << "mkdir(" << fs.c_str() << ")" << endl;
            errno = 0;
            mkdir(fs.c_str(), DEFAULT_MODE);
            if (errno != EEXIST && errno != 0)
            {
                perror("make_path:");
            }
        }
        // now handle the following path components
        while (path.getline(dir_component.data(), dir_component.size(), '/'))
        {
            string str_dircomp(dir_component.data());
            // cout << "k=" << k << "len=" << k.size() <<  endl;
            if (str_dircomp[0] == '/')
            {
                fs = fs + str_dircomp;
            }
            else
            {
                fs = fs + "/" + str_dircomp;
            }
            // dont try to make root
            if (fs == "/")
            {
                continue;
            }
            // cout << "mkdir(" << fs.c_str() << ")" << endl;
            errno = 0;
            mkdir(fs.c_str(), DEFAULT_MODE);
            if (errno != EEXIST && errno != 0)
            {
                perror("make_path");
            }
        }

    }

    void generate_log_filename(Time::Time_t time, string &fname)
    {
        int day, month, year, hours, minutes;
        double fseconds;
        Time::calendarDate(time, year, month, day, hours, minutes, fseconds);
        char buf[32];
        sprintf(buf,
                "%d_%02d_%02d_%02d:%02d:%02d",
                year,
                month,
                day,
                hours,
                minutes,
                (int) fseconds);
        fname = string(buf);
    }
}; // namespace matrix

// quick test cases
#if 0
int main(int, char **)
{

    make_path("./aa/py");
    make_path("ab/py");
    make_path("/tmp/ac/py");
    make_path("../tmp/ad/py");

    return 0;
}
#endif
