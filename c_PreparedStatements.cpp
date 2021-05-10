#include "c_PreparedStatements.hpp"

#define _PREPARE_STATEMENT 0x09
#define _EXECUTE_STATEMENT 0x0a
#define _RETRY_INSERTING 10

std::queue<PrepAndExecReq> q_PrepAndExecRequests;
std::mutex mtx_accessQueue;

bool bl_continueThread = true;

void* ConnPGSQLPrepStatements(void* arg) {    //Create the threads that will read the prepared statements requests
    //Definitions
    PGconn* connPrepState;
    PGresult* resPrepState;
    const char* conninfoPrepState = "user = postgres";
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
        logs("ConnPGSQLPrepStatements() : Secure search path error", ERROR);
    else
        logs("ConnPGSQLPrepStatements() : Connexion to PostgreSQL sucess");
    PQclear(resPrepState);
    printf("Yoloooooo\r\n");
    PrepExecStatement(connPrepState, arg);

    //At the end, closing
    PQfinish(connPrepState);
    logs("ConnPGSQLPrepStatements() : Fermeture du thread");
    pthread_exit(NULL);
}

void AddToQueue(PrepAndExecReq s_InReq) {
    while (!mtx_accessQueue.try_lock()) {};   //Add Data to queue. Mutex for thread safety
    q_PrepAndExecRequests.push(s_InReq);
    mtx_accessQueue.unlock();
    return;
}

void Ending() {
    bl_continueThread = false;
}

void PrepExecStatement(PGconn* connPrepState, void* arg) {
    PGresult* resCreateTable;

    const char* command;
    char defaultINSERT[] = "CALL pub_insert($1::varchar(24),$2::char(100),$3::char(100),$4::char(100),$5::char(100),$6::char(100),$7::char(100),$8::char(100),$9::char(100),$10::char(100),$11::char(100));";
    const char* paramValues[11];

    unsigned char ResponseToExecute_INSERT[13] = {      //CHANGED
        0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01
    };

    unsigned char ResponseToPrepare_INSERT[178] = {     //CHANGED
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
    char fieldDataExecute[10][100];
    char tableName[24];
    int cursor = 0;
    PrepAndExecReq s_Thr_PrepAndExec;

    command = defaultINSERT;

    while (bl_continueThread) {
        if (q_PrepAndExecRequests.size() > 0) {
            if (mtx_accessQueue.try_lock()) {
                if (q_PrepAndExecRequests.size() > 0) {
                    memcpy(&s_Thr_PrepAndExec.head[0], &q_PrepAndExecRequests.front().head[0], sizeof(s_Thr_PrepAndExec.head));
                    memcpy(&s_Thr_PrepAndExec.CQLStatement[0], &q_PrepAndExecRequests.front().CQLStatement[0], sizeof(s_Thr_PrepAndExec.CQLStatement));
                    s_Thr_PrepAndExec.origin = q_PrepAndExecRequests.front().origin;
                    q_PrepAndExecRequests.pop();
                    mtx_accessQueue.unlock();
                    if (s_Thr_PrepAndExec.head[4] == _PREPARE_STATEMENT) {
                        memcpy(&ResponseToPrepare_INSERT[2], &s_Thr_PrepAndExec.head[2], 2);
                        write(s_Thr_PrepAndExec.origin, ResponseToPrepare_INSERT, sizeof(ResponseToPrepare_INSERT));
                    }
                    else if (s_Thr_PrepAndExec.head[4] == _EXECUTE_STATEMENT) {
                        memcpy(&tableName[0], &s_Thr_PrepAndExec.CQLStatement[27], 24);
                        for (tableNameSize = 0; tableNameSize < 24; tableNameSize++) {
                            if (tableName[tableNameSize] == 0x00)
                                break;
                        }
                        cursor = tableNameSize + 27;
                        paramValues[0] = tableName;
                        logs(tableName);
                        for (int i = 0; i < 10; i++) {
                            memcpy(&fieldDataExecute[i], &s_Thr_PrepAndExec.CQLStatement[cursor + 4], sizeof(fieldDataExecute[i]) + 2);
                            paramValues[i + 1] = fieldDataExecute[i];
                            cursor += 104;
                        } 
                        //SYNC
                        resCreateTable = PQexecParams(connPrepState, command, 11, (const Oid*)NULL, paramValues, NULL, NULL, 0);
                        /*for(int i = 0; i < _RETRY_INSERTING; i++)
                        {
                            if ((PQresultStatus(resCreateTable) == PGRES_COMMAND_OK) || ((PQresultStatus(resCreateTable) == PGRES_FATAL_ERROR) && (std::string(PQerrorMessage(connPrepState)).substr(0,16) == "ERROR:  relation")))
                            {
                                memcpy(&ResponseToExecute_INSERT[2], &s_Thr_PrepAndExec.head[2], 2);
                                write(s_Thr_PrepAndExec.origin, &ResponseToExecute_INSERT, 13);
                                break;
                            }
                            else
                            {
                                PQclear(resCreateTable);
                                resCreateTable = PQexecParams(connPrepState, command, 11, (const Oid*)NULL, paramValues, NULL, NULL, 0);
                                // std::cout << "RETRY #" << i << std::endl;
                            }
                            if(i==_RETRY_INSERTING - 1)
                            {
                                logs(PQerrorMessage(connPrepState), ERROR);
                                std::cout << "!!! ECHEC !!!" << std::endl;
                            }
                        }
                        PQclear(resCreateTable);*/
                        memcpy(&ResponseToExecute_INSERT[2], &s_Thr_PrepAndExec.head[2], 2);
                        write(s_Thr_PrepAndExec.origin, &ResponseToExecute_INSERT, 13);
                        if (PQresultStatus(resCreateTable) == PGRES_COMMAND_OK)
                        {
                            PQclear(resCreateTable);
                        }
                        else
                        {
                            logs("!!! ECHEC !!!", ERROR);
                            PQclear(resCreateTable);
                        }
                        for (int i = 0; i < 10; i++)
                            memset(&fieldDataExecute[i], 0x00, sizeof(fieldDataExecute[i]));
                        memset(tableName, 0x00, sizeof(tableName));
                    }
                    else
                    {
                        printf("%x\r\n", s_Thr_PrepAndExec.head[4]);
                        logs("I GOT IT, FUCK FUCK FUCK", ERROR); 
                    }
                }
                else
                {
                    mtx_accessQueue.unlock();
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                }
            }
        }
        else
            std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
}
