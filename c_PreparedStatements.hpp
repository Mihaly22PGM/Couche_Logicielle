#ifndef C_PREPAREDSTATEMENTS_HPP
#define C_PREPAREDSTATEMENTS_HPP

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <list>
#include <future>
#include <string.h>
#include "libpq-fe.h"

struct PreparedReqStock{
    int ID;
    int Clause;
};
struct PreparedStatAndResponse{
    PGresult *PGReq;
    std::string Response;
};

void PrepStatementResponse(unsigned char[13], char[1024]);
// PreparedReqStock GetIDPreparedRequest();
// std::string InsertPrepStatement();
// PGresult PrepareInsertStatement();
#endif