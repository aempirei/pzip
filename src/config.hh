#pragma once

#include <list>
#include <string>

struct config {

    bool help      = false;
    bool keep      = false;
    bool force     = false;
    bool quiet     = false;
    bool verbose   = false;
    bool stdoutput = false;

    bool compress  = true;

    std::list<std::string> files;
    void usage(const char *) const;
    bool getopt(int, char **);
};
