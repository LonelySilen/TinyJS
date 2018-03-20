#include "log.h"

void log(const std::string& loginfo)
{
    std::cout << loginfo << std::endl;
}

void log_err(const std::string& loginfo)
{
    fprintf(stderr, "%s\n", loginfo.c_str());
    exit(1);
}

void log_err(const std::string& loginfo, const std::string& code, int line)
{
    fprintf(stderr, "%s, ", loginfo.c_str());
    fprintf(stderr, "[INFO] in line: %d, ", line);
    fprintf(stderr, "in token: %s.\n", code.c_str());
    exit(1);
}

void log_err(const std::string& loginfo, const std::string& code)
{
    fprintf(stderr, "%s, ", loginfo.c_str());
    fprintf(stderr, "{ info token: %s }\n", code.c_str());
    exit(1);
}