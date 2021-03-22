#include "c_Logs.h"

void c_Logs::logs(std::string msg, LogStatus LogStatusText)
{
    std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();    //Get time clock system
    tt =  std::chrono::system_clock::to_time_t(timestamp);
    t = ctime(&tt);
    if (t[strlen(t)-1] == '\n') t[strlen(t)-1] = '\0';  //Remove implicit '\n'

    std::ofstream file(fileLog, std::ios_base::app);
    if (file){
        if(LogStatusText == LogStatus::INFO)
            file<<t<<" : [INFO] "<<msg<<std::endl;
        else if(LogStatusText == LogStatus::ERROR)
            file<<t<<" : [ERROR] "<<msg<<std::endl;
        else
            file<<t<<" : [WARNING] "<<msg<<std::endl;
    }
    file.close();
}
void c_Logs::initClock(std::chrono::high_resolution_clock::time_point initStart){
    start = initStart;
}
void c_Logs::timestamp(std::string msg, std::chrono::high_resolution_clock::time_point stop){
    std::chrono::duration<double, std::micro> us_double = stop-start;   //std::micro
    std::ofstream file(fileLogPerfs, std::ios_base::app);
    if(file){
        file<<"TimeStamp : "<<uint64_t(us_double.count())/1000000<<" sec, "<<(uint64_t(us_double.count())/1000)%1000<<" ms, "<<uint64_t(us_double.count())%1000<<" us. Message : "<<msg<<std::endl;
    }
    file.close();
    start = std::chrono::high_resolution_clock::now();
}