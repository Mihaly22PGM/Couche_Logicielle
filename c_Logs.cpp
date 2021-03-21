#include "c_Logs.h"

// c_Logs::c_Logs(){};

void c_Logs::logs(std::string msg, LogStatus LogStatusText)
{
    std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();    //Get time clock system
    time_t tt =  std::chrono::system_clock::to_time_t(timestamp);
    std::ofstream file(fileLog, std::ios_base::app);
    if (file){
        file<<"Time : "<<ctime(&tt)<<" : "<<LogStatusText<<" : "<<msg<<std::endl;
    }
    file.close();
}