#include "c_PreparedStatements.hpp"
#include "c_Socket.hpp"

#define _NPOS std::string::npos
#define _PREPARE_STATEMENT 0x09
#define _EXECUTE_STATEMENT 0x0a

//#pragma region Global
std::queue<PrepAndExecReq> q_PrepAndExecRequests;
std::mutex mtx_accessQueue;
std::mutex mtx_writeOrder;
std::mutex mtx_writeClient;
bool bl_continueThread = true;
//#pragma endregion Global

//#pragma region Functions
void* ConnPGSQLPrepStatements(void* bl_loadPhase) {    //Create the threads that will read the prepared statements requests
    //Definitions
    PGconn *connPrepState;
    PGresult *resPrepState;
    const char *conninfoPrepState = "user = postgres";   
    //Execution
    connPrepState = PQconnectdb(conninfoPrepState);
    /* Check to see that the backend connection was successfully made */
    if (PQstatus(connPrepState) != CONNECTION_OK)
    {
        logs("ConnPGSQLPrepStatements() : Connexion to database failed", ERROR);
    }
    /* Set always-secure search path, so malicious users can't take control. */
    resPrepState = PQexec(connPrepState, "SELECT pg_catalog.set_config('search_path', 'public', false)");
    if (PQresultStatus(resPrepState) != PGRES_TUPLES_OK)
    {
        PQclear(resPrepState);
        logs("ConnPGSQLPrepStatements() : Secure search path error", ERROR);
    }
    else
        logs("ConnPGSQLPrepStatements() : Connexion to PostgreSQL sucess");
    PQclear(resPrepState);
    if((bool*)bl_loadPhase)
        PrepExecStatement(connPrepState);
    else
        PrepExecStatementRun(connPrepState);
    //At the end, closing
    PQfinish(connPrepState);
    logs("ConnPGSQLPrepStatements() : Fermeture du thread");
    pthread_exit(NULL);
}

void AddToQueue(PrepAndExecReq s_InReq){
    while(!mtx_accessQueue.try_lock()){};   //Add Data to queue. Mutex for thread safety
    q_PrepAndExecRequests.push(s_InReq);
    mtx_accessQueue.unlock();
    return;
}
void Ending(){
    bl_continueThread = false;
}

void PrepExecStatement(PGconn *connPrepState){
    char createTable_Req[] = "CREATE TABLE ";
    char createTableColumns_Req[] = " (column_name char(6), value char(100));";
    char prepINSERT_Req[] = "INSERT INTO ";
    char prepINSERTColumns_Req[] =  " (column_name, value) VALUES ($1::char(6),$2::char(100));";
    char desalocateReq[] = "DEALLOCATE ";
    PGresult *prep;
    PGresult *prepExec;
    PGresult *resCreateTable;
    PrepAndExecReq s_Thr_PrepAndExec;
    char stockPrepReq[10] = {0x31, 0x30, 0x37, 0x36, 0x39, 0x38, 0x33, 0x32, 0x35, 0x34};
    char* paramValues[2] = {"fieldx", ""};
    char unsigned PreparedReqID[18];
    unsigned char ResponseToExecute[13] = {0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01}; 
    unsigned char ResponseToPrepInsert[178] = {
        0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0xa9, 0x00, 0x00, 0x00, 0x04, 0x00,
        0x10, 0x59, 0xed, 0x03, 0xd8, 0x08, 0x5b, 0x75, 0xf4, 0x31, 0x1f, 0x57, 0xc7, 0x34, 0x36, 0xd9,
        0xbc, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x04, 0x79, 0x63, 0x73, 0x62, 0x00, 0x09, 0x75, 0x73, 0x65, 0x72, 0x74, 0x61, 0x62, 0x6c, 0x65,
        0x00, 0x04, 0x79, 0x5f, 0x69, 0x64, 0x00, 0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x31,
        0x00, 0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x30, 0x00, 0x0d, 0x00, 0x06, 0x66, 0x69,
        0x65, 0x6c, 0x64, 0x37, 0x00, 0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x36, 0x00, 0x0d,
        0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x39, 0x00, 0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c,
        0x64, 0x38, 0x00, 0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x33, 0x00, 0x0d, 0x00, 0x06,
        0x66, 0x69, 0x65, 0x6c, 0x64, 0x32, 0x00, 0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x35,
        0x00, 0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x34, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x04,
        0x00, 0x00, 0x00, 0x00

    };
    int tableNameSize = 0;
    char createTable[85];
    char prepreq[91];
    char remPrepReq[100];
    char fieldColumnExecute[] = "fieldx";
    char fieldDataExecute[100];
    char tableName[24];
    int cursor = 0;
    int fieldOrder = 0;
    int paramLengths[2] = {6, 100};
    int paramFormats[2] = {0, 0};

    while(bl_continueThread){
        if(q_PrepAndExecRequests.size()>0){
            if(mtx_accessQueue.try_lock()){
                if(q_PrepAndExecRequests.size()>0){
                    memcpy(&s_Thr_PrepAndExec.head[0], &q_PrepAndExecRequests.front().head[0], sizeof(s_Thr_PrepAndExec.head));
                    memcpy(&s_Thr_PrepAndExec.CQLStatement[0], &q_PrepAndExecRequests.front().CQLStatement[0], sizeof(s_Thr_PrepAndExec.CQLStatement));
                    q_PrepAndExecRequests.pop();
                    mtx_accessQueue.unlock();
                    if(s_Thr_PrepAndExec.head[4] == _PREPARE_STATEMENT){
                        // try{
                            memcpy(&ResponseToPrepInsert[2], &s_Thr_PrepAndExec.head[2], 2);
                            write(GetSocket(), ResponseToPrepInsert, sizeof(ResponseToPrepInsert));
                        // }
                        // catch (std::exception const& e) {
                        //     logs("PrepStatementResponse() : " + std::string(e.what()), ERROR);
                        // }
                    }
                    else if(s_Thr_PrepAndExec.head[4] == _EXECUTE_STATEMENT){
                        // try{
                            // timestamp("Start Exec "+ idThread, std::chrono::high_resolution_clock::now());
                            memcpy(PreparedReqID, &s_Thr_PrepAndExec.CQLStatement[0], 18);
                            memcpy(&tableName[0], &s_Thr_PrepAndExec.CQLStatement[27], 24);
                            for(tableNameSize=0; tableNameSize < 24; tableNameSize++){
                                if(tableName[tableNameSize] == 0x00){
                                    break;
                                }
                            }
                            memset(createTable, 0x00, sizeof(createTable)); //Reset requests
                            memset(prepreq, 0x00, sizeof(prepreq));
                            memset(remPrepReq, 0x00, sizeof(remPrepReq));
                            memcpy(&createTable[0],&createTable_Req[0],sizeof(createTable_Req));    //COPY clauses
                            memcpy(&prepreq[0],&prepINSERT_Req[0],sizeof(prepINSERT_Req));
                            memcpy(&remPrepReq[0], &desalocateReq[0], sizeof(desalocateReq));
                            memcpy(&createTable[13],&tableName[0],tableNameSize);   //COPY table name
                            memcpy(&prepreq[12],&tableName[0],tableNameSize);
                            memcpy(&remPrepReq[11], &tableName[0], tableNameSize);
                            memcpy(&createTable[13+tableNameSize],&createTableColumns_Req[0],sizeof(createTableColumns_Req));   //COPY columns name
                            memcpy(&prepreq[12+tableNameSize],&prepINSERTColumns_Req[0],sizeof(prepINSERTColumns_Req));
                            // timestamp("PG comm start "+ idThread, std::chrono::high_resolution_clock::now());           //PERF OK
                            memcpy(&ResponseToExecute[2], &s_Thr_PrepAndExec.head[2], 2);
                            write(GetSocket(), &ResponseToExecute, 13);
                            resCreateTable = PQexec(connPrepState, createTable);
                            if (PQresultStatus(resCreateTable) == PGRES_COMMAND_OK)
                            {
                                PQclear(resCreateTable);

                                prep = PQprepare(connPrepState,tableName, prepreq, 2, (const Oid)NULL);
                                if (PQresultStatus(prep) == PGRES_COMMAND_OK){
                                    // logsPGSQL(prepreq);
                                    cursor = tableNameSize+27;
                                    fieldOrder = 0;
                                    // while(s_Thr_PrepAndExec.CQLStatement[cursor] == 0x00 && s_Thr_PrepAndExec.CQLStatement[cursor+1] == 0x00 && s_Thr_PrepAndExec.CQLStatement[cursor+2] == 0x00 && s_Thr_PrepAndExec.CQLStatement[cursor+3] == 0x64 && fieldOrder<10){
                                    //     memcpy(&fieldDataExecute[fieldOrder][0], &s_Thr_PrepAndExec.CQLStatement[cursor+4], sizeof(fieldDataExecute[fieldOrder]));
                                    //     fieldColumnExecute[5] = stockPrepReq[fieldOrder];
                                    //     paramValues[0] = fieldColumnExecute;
                                    //     paramValues[1] = fieldDataExecute[fieldOrder];
                                    //     fieldOrder++;
                                    //     PQsendQueryPrepared(connPrepState, tableName, 2, paramValues, paramLengths, paramFormats, 0);
                                    //     cursor +=104;
                                    //     //PQclear(prepExec);
                                    // }
                                    while(s_Thr_PrepAndExec.CQLStatement[cursor] == 0x00 && s_Thr_PrepAndExec.CQLStatement[cursor+1] == 0x00 && s_Thr_PrepAndExec.CQLStatement[cursor+2] == 0x00 && s_Thr_PrepAndExec.CQLStatement[cursor+3] == 0x64 && fieldOrder<10){
                                        memcpy(fieldDataExecute, &s_Thr_PrepAndExec.CQLStatement[cursor+4], sizeof(fieldDataExecute));
                                        fieldColumnExecute[5] = stockPrepReq[fieldOrder];
                                        paramValues[0] = fieldColumnExecute;
                                        paramValues[1] = fieldDataExecute;
                                        fieldOrder++;
                                        prepExec = PQexecPrepared(connPrepState, tableName, 2, paramValues, paramLengths, paramFormats, 0);
                                        // if (PQresultStatus(prepExec) == PGRES_COMMAND_OK){
                                        //     if(fieldOrder == 9){
                                        //         memcpy(&ResponseToExecute[2], &s_Thr_PrepAndExec.head[2], 2);
                                        //         write(GetSocket(), &ResponseToExecute, 13);
                                        //     }
                                        // }
                                        // else{
                                        //     // printf("HERE?\r\n");
                                        //     // logsPGSQL(PQresultErrorMessage(prepExec), ERROR);
                                        //     memcpy(&ResponseToExecute[2], &s_Thr_PrepAndExec.head[2], 2);
                                        //     write(GetSocket(), &ResponseToExecute, 13);
                                        //     break;
                                        // }
                                        cursor +=104;
                                        PQclear(prepExec);
                                    }
                                }
                                else{
                                    logsPGSQL(PQresultErrorMessage(prep), ERROR);
                                }
                                PQclear(prep);
                                PQsendQuery(connPrepState, remPrepReq);
                            }
                            else{
                                logsPGSQL(createTable, ERROR);
                            }
                            memset(tableName, 0x00, sizeof(tableName));
                            // timestamp("Exec time "+ idThread, std::chrono::high_resolution_clock::now());
                        // }
                        // catch (std::exception const& e) {
                        //     logs("ExecutePrepStatement() : " + std::string(e.what()), ERROR);
                        // }
                    }
                }
                else{
                    mtx_accessQueue.unlock();
                }
            }
        }
        else{
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void PrepExecStatementRun(PGconn *connPrepState){
    /*char Request_SELECT_ALL[] = "SELECT ALL FROM $1;";
    char Request_UPDATE[] = "UPDATE $1 SET value = $2 WHERE column_name = $3;";
    char createTable_Req[] = "CREATE TABLE $1 (column_name varchar(255), value varchar(255));";
    char prepINSERT_Req[] = "INSERT INTO $1 (column_name, value) VALUES ($2::varchar,$3::varchar);";
    char desalocateReq[] = "DEALLOCATE $1;";
    char prepID[22];    
    unsigned char Prep_ID[12][18] = {
        {0x00, 0x10, 0x60, 0x2c, 0xd3, 0xc1, 0x65, 0x95, 0x8a, 0xd7, 0x92, 0xc9, 0x2d, 0xb2, 0x0f, 0x7c, 0x58, 0xf5},
        {0x00, 0x10, 0x61, 0x2c, 0xd3, 0xc1, 0x65, 0x95, 0x8a, 0xd7, 0x92, 0xc9, 0x2d, 0xb2, 0x0f, 0x7c, 0x58, 0xf5},
        {0x00, 0x10, 0x62, 0x2c, 0xd3, 0xc1, 0x65, 0x95, 0x8a, 0xd7, 0x92, 0xc9, 0x2d, 0xb2, 0x0f, 0x7c, 0x58, 0xf5},
        {0x00, 0x10, 0x63, 0x2c, 0xd3, 0xc1, 0x65, 0x95, 0x8a, 0xd7, 0x92, 0xc9, 0x2d, 0xb2, 0x0f, 0x7c, 0x58, 0xf5},
        {0x00, 0x10, 0x64, 0x2c, 0xd3, 0xc1, 0x65, 0x95, 0x8a, 0xd7, 0x92, 0xc9, 0x2d, 0xb2, 0x0f, 0x7c, 0x58, 0xf5},
        {0x00, 0x10, 0x65, 0x2c, 0xd3, 0xc1, 0x65, 0x95, 0x8a, 0xd7, 0x92, 0xc9, 0x2d, 0xb2, 0x0f, 0x7c, 0x58, 0xf5},
        {0x00, 0x10, 0x66, 0x2c, 0xd3, 0xc1, 0x65, 0x95, 0x8a, 0xd7, 0x92, 0xc9, 0x2d, 0xb2, 0x0f, 0x7c, 0x58, 0xf5},
        {0x00, 0x10, 0x67, 0x2c, 0xd3, 0xc1, 0x65, 0x95, 0x8a, 0xd7, 0x92, 0xc9, 0x2d, 0xb2, 0x0f, 0x7c, 0x58, 0xf5},
        {0x00, 0x10, 0x68, 0x2c, 0xd3, 0xc1, 0x65, 0x95, 0x8a, 0xd7, 0x92, 0xc9, 0x2d, 0xb2, 0x0f, 0x7c, 0x58, 0xf5},
        {0x00, 0x10, 0x69, 0x2c, 0xd3, 0xc1, 0x65, 0x95, 0x8a, 0xd7, 0x92, 0xc9, 0x2d, 0xb2, 0x0f, 0x7c, 0x58, 0xf5},
        {0x00, 0x10, 0x59, 0xed, 0x03, 0xd8, 0x08, 0x5b, 0x75, 0xf4, 0x31, 0x1f, 0x57, 0xc7, 0x34, 0x36, 0xd9, 0xbc},
        {0x00, 0x10, 0xee, 0xe3, 0x5a, 0x0a, 0xe7, 0x33, 0xd4, 0x3e, 0x68, 0xeb, 0x2b, 0xc4, 0xae, 0xa2, 0x3d, 0xed},
    };
    PGresult *prep;
    PGresult *prepExec;
    PGresult *resCreateTable;
    PrepAndExecReq s_Thr_PrepAndExec;
    char ResponseBuff[2048];
    char stockPrepReq[10] = {0x31, 0x30, 0x37, 0x36, 0x39, 0x38, 0x33, 0x32, 0x35, 0x34};
    char* paramValues[2] = {"fieldx", ""};
    char PreparedReqID[18];
    unsigned char ResponseToExecute[13] = {0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01}; 
    unsigned char ResponseToPrepInsert[178] = {
        0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0xa9, 0x00, 0x00, 0x00, 0x04, 0x00,
        0x10, 0x59, 0xed, 0x03, 0xd8, 0x08, 0x5b, 0x75, 0xf4, 0x31, 0x1f, 0x57, 0xc7, 0x34, 0x36, 0xd9,
        0xbc, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x04, 0x79, 0x63, 0x73, 0x62, 0x00, 0x09, 0x75, 0x73, 0x65, 0x72, 0x74, 0x61, 0x62, 0x6c, 0x65,
        0x00, 0x04, 0x79, 0x5f, 0x69, 0x64, 0x00, 0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x31,
        0x00, 0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x30, 0x00, 0x0d, 0x00, 0x06, 0x66, 0x69,
        0x65, 0x6c, 0x64, 0x37, 0x00, 0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x36, 0x00, 0x0d,
        0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x39, 0x00, 0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c,
        0x64, 0x38, 0x00, 0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x33, 0x00, 0x0d, 0x00, 0x06,
        0x66, 0x69, 0x65, 0x6c, 0x64, 0x32, 0x00, 0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x35,
        0x00, 0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x34, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x04,
        0x00, 0x00, 0x00, 0x00
    };
    unsigned char ResponseToPrepSelect[203] = {
        0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0xc2, 0x00, 0x00, 0x00, 0x04, 0x00,
        0x10, 0xee, 0xe3, 0x5a, 0x0a, 0xe7, 0x33, 0xd4, 0x3e, 0x68, 0xeb, 0x2b, 0xc4, 0xae, 0xa2, 0x3d,
        0xed, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x04, 0x79, 0x63, 0x73, 0x62, 0x00, 0x09, 0x75, 0x73, 0x65, 0x72, 0x74, 0x61, 0x62, 0x6c, 0x65,
        0x00, 0x04, 0x79, 0x5f, 0x69, 0x64, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0b,
        0x00, 0x04, 0x79, 0x63, 0x73, 0x62, 0x00, 0x09, 0x75, 0x73, 0x65, 0x72, 0x74, 0x61, 0x62, 0x6c,
        0x65, 0x00, 0x04, 0x79, 0x5f, 0x69, 0x64, 0x00, 0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64,
        0x30, 0x00, 0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x31, 0x00, 0x0d, 0x00, 0x06, 0x66,
        0x69, 0x65, 0x6c, 0x64, 0x32, 0x00, 0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x33, 0x00,
        0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x34, 0x00, 0x0d, 0x00, 0x06, 0x66, 0x69, 0x65,
        0x6c, 0x64, 0x35, 0x00, 0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x36, 0x00, 0x0d, 0x00,
        0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x37, 0x00, 0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64,
        0x38, 0x00, 0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x39, 0x00, 0x0d
    };
    unsigned char ResponseToUpdate[88] = {
        0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x4f, 0x00, 0x00, 0x00,
        0x04, 0x00, 0x10, 0x96, 0x2c, 0xd3, 0xc1, 0x65, 0x95, 0x8a, 0xd7, 0x92, 0xc9, 0x2d, 0xb2, 0x0f,
        0x7c, 0x58, 0xf5, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00,
        0x01, 0x00, 0x04, 0x79, 0x63, 0x73, 0x62, 0x00, 0x09, 0x75, 0x73, 0x65, 0x72, 0x74, 0x61, 0x62,
        0x6c, 0x65, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x35, 0x00, 0x0d, 0x00, 0x04, 0x79, 0x5f,
        0x69, 0x64, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00};
    int tableNameSize = 0;
    char createTable[85];
    char prepreq[91];
    char remPrepReq[100];
    char fieldColumnExecute[] = "fieldx";
    char fieldDataExecute[100];
    char tableName[24];
    int cursor = 0;
    int fieldOrder = 0;
    int paramLengths[2] = {6, 100};
    int paramFormats[2] = {0, 0};
    int cursorFrame=0;
    std::string Temp_Req = "";
    memset(&prepID[0], 0x00, sizeof(prepID));
    memset(&ResponseBuff[0], 0x00, sizeof(ResponseBuff));
    prepID[3] = 0x04;
    prepID[4] = 0x10;
    prepID[21] = 0x04;
    int UPDATE_CHAMP = 0;   
    while(bl_continueThread){
        if(q_PrepAndExecRequests.size()>0){
            if(mtx_accessQueue.try_lock()){
                if(q_PrepAndExecRequests.size()>0){
                    memcpy(&s_Thr_PrepAndExec.head[0], &q_PrepAndExecRequests.front().head[0], sizeof(s_Thr_PrepAndExec.head));
                    memcpy(&s_Thr_PrepAndExec.CQLStatement[0], &q_PrepAndExecRequests.front().CQLStatement[0], sizeof(s_Thr_PrepAndExec.CQLStatement));
                    q_PrepAndExecRequests.pop();
                    mtx_accessQueue.unlock();
                    if(s_Thr_PrepAndExec.head[4] == _EXECUTE_STATEMENT){
                        try{
                            // timestamp("Start Exec "+ idThread, std::chrono::high_resolution_clock::now());
                            memcpy(PreparedReqID, &s_Thr_PrepAndExec.CQLStatement[0], 18);
                            memcpy(tableName, &s_Thr_PrepAndExec.CQLStatement[27], 24);
                            for(tableNameSize=0; tableNameSize < 24; tableNameSize++){
                                if(tableName[tableNameSize] == 0x00){
                                    break;
                                }
                            }
                            if(PreparedReqID[3]>=0x60 && PreparedReqID[3]<70){  //UPDATE

                            }
                            else if(PreparedReqID[3] == 0x59){                  //INSERT

                            }
                            else if(PreparedReqID[3] == 0xee){                  //SELECT

                            }
                            else{
                                printf("PreparedReqID[3] : 0x%x\r\n", PreparedReqID[3]);
                                logs("Erreur dans l'ID de la requête", ERROR);
                            }
                            
                            memset(createTable, 0x00, sizeof(createTable)); //Reset requests
                            memset(prepreq, 0x00, sizeof(prepreq));
                            memset(remPrepReq, 0x00, sizeof(remPrepReq));
                            memcpy(&createTable[0],&createTable_Req[0],sizeof(createTable_Req));    //COPY clauses
                            memcpy(&prepreq[0],&prepINSERT_Req[0],sizeof(prepINSERT_Req));
                            memcpy(&remPrepReq[0], &desalocateReq[0], sizeof(desalocateReq));
                            memcpy(&createTable[13],&tableName[0],tableNameSize);   //COPY table name
                            memcpy(&prepreq[12],&tableName[0],tableNameSize);
                            memcpy(&remPrepReq[11], &tableName[0], tableNameSize);
                            memcpy(&createTable[13+tableNameSize],&createTableColumns_Req[0],sizeof(createTableColumns_Req));   //COPY columns name
                            memcpy(&prepreq[12+tableNameSize],&prepINSERTColumns_Req[0],sizeof(prepINSERTColumns_Req));
                            // timestamp("PG comm start "+ idThread, std::chrono::high_resolution_clock::now());           //PERF OK
                            memcpy(&ResponseToExecute[2], &s_Thr_PrepAndExec.head[2], 2);
                            write(GetSocket(), &ResponseToExecute, 13);
                            resCreateTable = PQexec(connPrepState, createTable);
                           
                            if (PQresultStatus(resCreateTable) == PGRES_COMMAND_OK)
                            {
                                PQclear(resCreateTable);

                                prep = PQprepare(connPrepState,tableName, prepreq, 2, (const Oid)NULL); //TODO maybe tableName not good, to check and maybe delete prep requests after if needed
                                if (PQresultStatus(prep) == PGRES_COMMAND_OK){
                                    // logsPGSQL(prepreq);
                                    cursor = tableNameSize+27;
                                    fieldOrder = 0;
                                    // while(s_Thr_PrepAndExec.CQLStatement[cursor] == 0x00 && s_Thr_PrepAndExec.CQLStatement[cursor+1] == 0x00 && s_Thr_PrepAndExec.CQLStatement[cursor+2] == 0x00 && s_Thr_PrepAndExec.CQLStatement[cursor+3] == 0x64 && fieldOrder<10){
                                    //     memcpy(&fieldDataExecute[fieldOrder][0], &s_Thr_PrepAndExec.CQLStatement[cursor+4], sizeof(fieldDataExecute[fieldOrder]));
                                    //     fieldColumnExecute[5] = stockPrepReq[fieldOrder];
                                    //     paramValues[0] = fieldColumnExecute;
                                    //     paramValues[1] = fieldDataExecute[fieldOrder];
                                    //     fieldOrder++;
                                    //     PQsendQueryPrepared(connPrepState, tableName, 2, paramValues, paramLengths, paramFormats, 0);
                                    //     cursor +=104;
                                    //     //PQclear(prepExec);
                                    // }
                                    while(s_Thr_PrepAndExec.CQLStatement[cursor] == 0x00 && s_Thr_PrepAndExec.CQLStatement[cursor+1] == 0x00 && s_Thr_PrepAndExec.CQLStatement[cursor+2] == 0x00 && s_Thr_PrepAndExec.CQLStatement[cursor+3] == 0x64 && fieldOrder<10){
                                        memcpy(fieldDataExecute, &s_Thr_PrepAndExec.CQLStatement[cursor+4], sizeof(fieldDataExecute));
                                        fieldColumnExecute[5] = stockPrepReq[fieldOrder];
                                        paramValues[0] = fieldColumnExecute;
                                        paramValues[1] = fieldDataExecute;
                                        fieldOrder++;
                                        prepExec = PQexecPrepared(connPrepState, tableName, 2, paramValues, paramLengths, paramFormats, 0);
                                        if (PQresultStatus(prepExec) == PGRES_COMMAND_OK){
                                            if(fieldOrder == 9){
                                                // memcpy(&ResponseToExecute[2], &s_Thr_PrepAndExec.head[2], 2);
                                                // write(GetSocket(), &ResponseToExecute, 13);
                                            }
                                        }
                                        else{
                                            logsPGSQL(PQresultErrorMessage(prepExec), ERROR);
                                            memcpy(&ResponseToExecute[2], &s_Thr_PrepAndExec.head[2], 2);
                                            write(GetSocket(), &ResponseToExecute, 13);
                                            break;
                                        }
                                        cursor +=104;
                                        PQclear(prepExec);
                                    }
                                }
                                else{
                                    logsPGSQL(PQresultErrorMessage(prep), ERROR);
                                }
                                PQclear(prep);
                                PQsendQuery(connPrepState, remPrepReq);
                            }
                            else{
                                logsPGSQL(createTable, ERROR);
                            }
                            memset(tableName, 0x00, sizeof(tableName));
                            // timestamp("Exec time "+ idThread, std::chrono::high_resolution_clock::now());
                        }
                        catch (std::exception const& e) {
                            logs("ExecutePrepStatement() : " + std::string(e.what()), ERROR);
                        }
                    }
                    else if(s_Thr_PrepAndExec.head[4] == _PREPARE_STATEMENT){
                        try{
                            Temp_Req = (std::string((char*)s_Thr_PrepAndExec.CQLStatement)).substr(0,6);
                            if(Temp_Req == "SELECT")
                            {
                                memcpy(&ResponseToPrepSelect[2], &s_Thr_PrepAndExec.head[2], 2);
                                write(GetSocket(), ResponseToPrepSelect, sizeof(ResponseToPrepSelect));
                            }
                            else if(Temp_Req == "UPDATE"){
                                memcpy(&ResponseToUpdate[2], &s_Thr_PrepAndExec.head[2], 2);
                                UPDATE_CHAMP = ResponseToUpdate[69] = s_Thr_PrepAndExec.CQLStatement[26];
                                UPDATE_CHAMP -=48;  //Convert aasci number to int
                                if(UPDATE_CHAMP <10 && UPDATE_CHAMP>=0){
                                    memcpy(&ResponseToUpdate[13], &Prep_ID[UPDATE_CHAMP][0], sizeof(Prep_ID[UPDATE_CHAMP]));
                                    write(GetSocket(), ResponseToUpdate, sizeof(ResponseToPrepSelect));
                                }
                                else 
                                    logs("UPDATE sur un champ non reconnu", ERROR);
                            }
                            else if(Temp_Req == "INSERT"){
                                memcpy(&ResponseToPrepInsert[2], &s_Thr_PrepAndExec.head[2], 2);
                                write(GetSocket(), ResponseToPrepInsert, sizeof(ResponseToPrepInsert));
                            }
                            else
                                logs("Erreur dans la requete", ERROR);
                        }
                        catch (std::exception const& e) {
                            logs("PrepStatementResponse() : " + std::string(e.what()), ERROR);
                        }
                    }
                }
                else{
                    mtx_accessQueue.unlock();
                }
            }
        }
        else{
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }*/
}

//#pragma endregion Functions
