#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <stdio.h>
#include <time.h>
#include <chrono>
#include <fstream>
#include <string.h>

#ifndef C_LOGS_H
#define C_LOGS_H
class c_Logs
{
private:
    const std::string fileLog = "Logs";
    const std::string fileLogPerfs = "LogsPerfs";
    time_t tt;
    char *t;
    std::chrono::time_point<std::chrono::high_resolution_clock> previous;
    double diff;
    clock_t start = 0;
public:
    enum LogStatus {INFO, WARNING, ERROR};
    void logs(std::string, LogStatus LogStatusText = LogStatus::INFO);
    void timestamp(std::string, clock_t);
};

#endif