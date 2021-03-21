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
    time_t tt;
    char *t;
public:
    enum LogStatus {INFO, WARNING, ERROR};
    void logs(std::string msg, LogStatus LogStatusText = LogStatus::INFO);
};

#endif