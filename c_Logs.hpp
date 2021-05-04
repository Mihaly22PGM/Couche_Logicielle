#ifndef C_LOGS_HPP
#define C_LOGS_HPP

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <time.h>
#include <chrono>
#include <fstream>
#include <string.h>

enum LogStatus { INFO, WARNING, ERROR };
void logs(std::string, LogStatus LogStatusText = LogStatus::INFO);
void initClock(std::chrono::high_resolution_clock::time_point);
void timestamp(std::string, std::chrono::high_resolution_clock::time_point);
void logsPGSQL(std::string, LogStatus LogStatusText = LogStatus::INFO);

#endif