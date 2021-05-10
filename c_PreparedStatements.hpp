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
#include "c_Socket.hpp"

struct PrepAndExecReq {
    unsigned char head[13];
    unsigned char CQLStatement[2048];
    int origin;
};

struct server
{
    std::string server_name;
    int server_id;
    std::string server_ip_address;
};

void* ConnPGSQLPrepStatements(void*);
void AddToQueue(PrepAndExecReq);
void PrepExecStatement(PGconn*, void* arg = NULL);
void Ending();
#endif