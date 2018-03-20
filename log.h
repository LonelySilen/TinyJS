#ifndef TS_LOG
#define TS_LOG

#include <string>
#include <cstdlib>
#include <iostream>

void log(const std::string& loginfo);
void log_err(const std::string& loginfo);
void log_err(const std::string& loginfo, const std::string& code);
void log_err(const std::string& loginfo, const std::string& code, int line);

#endif