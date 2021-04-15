#ifndef C_PREPAREDSTATEMENTS_HPP
#define C_PREPAREDSTATEMENTS_HPP

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <list>
#include <vector>
#include <pthread.h>
#include <future>
#include <mutex>
#include <queue>
#include <string.h>
#include <sstream>
#include "libpq-fe.h"
#include "c_Logs.hpp"

struct PreparedReqStock{
    char ID[16];
    char order[10];
};
struct PreparedStatAndResponse{
    PGresult *PGReq;
    std::string Response;
};
struct SQLPrepRequests {
    char stream[2];
    std::string request;
};
struct PrepAndExecReq{
    unsigned char head[13];
    unsigned char CQLStatement[2048];
};

void* ConnPGSQLPrepStatements(void*);
void AddToQueue(PrepAndExecReq);
void PrepExecStatement(PGconn*);
void Ending();
#endif