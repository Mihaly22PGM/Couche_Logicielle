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
struct server
{
    std::string server_name;
    int server_id;
    std::string server_ip_address;
};
struct replication_relation     //ensemble de deux serveurs pour la réplication
{
    server publisher;
    server subscriber;
    int th_num;
};
struct PrepAndExecReq{
    unsigned char head[13];
    unsigned char CQLStatement[2048];
    int origin;
};

void* ConnPGSQLPrepStatements(void*);
void AddToQueue(PrepAndExecReq);
void PrepExecStatement(PGconn*, void* arg = NULL);
void Ending();
#endif