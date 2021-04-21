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
std::mutex mtx_getID;

bool bl_continueThread = true;
int ID_Thread = 0;

const char* conninfoPrepState = "user = postgres";
const char connection_string_host[] = "host = ";      //...destination_server.server_ip_address
const char connection_string_user[] = " user = postgres password = Gang-bang69";

const char createTable_Req[] = "CREATE TABLE ";
const char createTableColumns_Req[] = " (column_name varchar(255), value varchar(255));";
const char prepINSERT_Req[] = "INSERT INTO ";
const char prepINSERTColumns_Req[] = " (column_name, value) VALUES ($1::varchar,$2::varchar);";
const char desalocateReq[] = "DEALLOCATE ";

//ADDED
const char selectFrom_Req[] = "SELECT * FROM ";

const char update_Req[] = "UPDATE ";
const char updateSetValue_Req[] = " SET value = '";
const char updateFieldX_Req[] = "' WHERE column_name = 'fieldx';";
//ENDADDED

/*char createPublication_Req[] = "CREATE PUBLICATION publication_";       //...origin_server.server_name
char createPublicationTables_Req[] = " FOR TABLE ";     //...tableName
char creatSubscription_Req[] = "CREATE SUBSCRIPTION subscription_";         //...destination_server.server_name
char creatSubscriptionConnection_Req[] = " CONNECTION 'host=";      //...origin_server.server_ip_address
char creatSubscriptionPublication_Req[] = " user=postgres dbname=postgres password=Gang-bang69' PUBLICATION publication_";       //...origin_server.server_name*/
const char alterPublication_Req[] = "ALTER PUBLICATION publication_";      //...origin_server.server_name
const char alterPublicationAdd_Req[] = " ADD TABLE ";     //...tableName
const char alterSubscription_Req[] = "ALTER SUBSCRIPTION subscription_";      //..destination_server.server_name
const char alterSubscriptionRefresh_Req[] = " REFRESH PUBLICATION;";

const char stockPrepReq[10] = { 0x31, 0x30, 0x37, 0x36, 0x39, 0x38, 0x33, 0x32, 0x35, 0x34 };

const int paramLengths[2] = { 6, 100 };
const int paramFormats[2] = { 0, 0 };
//#pragma endregion Global

//#pragma region Functions
void* ConnPGSQLPrepStatements(void* arg) {    //Create the threads that will read the prepared statements requests 
    //Definitions
    PGconn* connPrepState;
    PGresult* resPrepState;
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

    if (arg != NULL)
    {
        replication_relation* replication_servers = (replication_relation*)arg;
        server origin_server = replication_servers->publisher;
        std::cout << "Serveur origine: " << origin_server.server_id << " | " << origin_server.server_ip_address << " | " << origin_server.server_name << std::endl;
        server destination_server = replication_servers->subscriber;
        std::cout << "Serveur destination: " << destination_server.server_id << " | " << destination_server.server_ip_address << " | " << destination_server.server_name << std::endl;

        PGconn* replic_connPrepState;
        PGresult* replic_resPrepState;

        char replic_conninfoPrepState[65];
        memcpy(&replic_conninfoPrepState[0], &connection_string_host[0], sizeof(connection_string_host) - 1);
        memcpy(&replic_conninfoPrepState[sizeof(connection_string_host) - 1], &(destination_server.server_ip_address.c_str())[0], destination_server.server_ip_address.length());
        memcpy(&replic_conninfoPrepState[sizeof(connection_string_host) - 1 + destination_server.server_ip_address.length()], &connection_string_user[0], sizeof(connection_string_user) - 1);
        /*for(int i = 0; i < sizeof(replic_conninfoPrepState); i++)
            std::cout << replic_conninfoPrepState[i];*/
            //Execution
        replic_connPrepState = PQconnectdb(replic_conninfoPrepState);
        /* Check to see that the backend connection was successfully made */
        if (PQstatus(replic_connPrepState) != CONNECTION_OK)
        {
            logs("ConnPGSQLPrepStatements() : Connexion to database failed", ERROR);
        }
        /* Set always-secure search path, so malicious users can't take control. */
        replic_resPrepState = PQexec(replic_connPrepState, "SELECT pg_catalog.set_config('search_path', 'public', false)");
        if (PQresultStatus(replic_resPrepState) != PGRES_TUPLES_OK)
        {
            PQclear(replic_resPrepState);
            logs("ConnPGSQLPrepStatements() : Secure search path error", ERROR);
        }
        else
        {
            //std::cout << "Connecte au subscriber" << std::endl;
            logs("ConnPGSQLPrepStatements() : Connexion to PostgreSQL sucess");
        }
        PQclear(replic_resPrepState);

        PrepExecStatement(connPrepState, replic_connPrepState, origin_server, destination_server);
        PQfinish(replic_connPrepState);
    }
    else
        PrepExecStatement(connPrepState);
    //At the end, closing
    PQfinish(connPrepState);
    // delete conninfoPrepState;
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

void PrepExecStatement(PGconn* connPrepState) {
    PGresult* prep;
    PGresult* prepExec;
    PGresult* resCreateTable;
    PrepAndExecReq s_Thr_PrepAndExec;
    char* paramValues[2] = { "fieldx", "" };
    char unsigned PreparedReqID[18];
    unsigned char ResponseToExecute[13] = { 0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01 };
    unsigned char ResponseToPrepare[178] = {
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
    // std::ostringstream ss;
    // ss <<  std::this_thread::get_id();
    // std::string idThread = ss.str();
    while (bl_continueThread) {
        if (q_PrepAndExecRequests.size() > 0) {
            if (mtx_accessQueue.try_lock()) {
                if (q_PrepAndExecRequests.size() > 0) {
                    printf("%zu\r\n", q_PrepAndExecRequests.size());
                    memcpy(&s_Thr_PrepAndExec.head[0], &q_PrepAndExecRequests.front().head[0], sizeof(s_Thr_PrepAndExec.head));
                    memcpy(&s_Thr_PrepAndExec.CQLStatement[0], &q_PrepAndExecRequests.front().CQLStatement[0], sizeof(s_Thr_PrepAndExec.CQLStatement));
                    s_Thr_PrepAndExec.origin = q_PrepAndExecRequests.front().origin;
                    q_PrepAndExecRequests.pop();
                    mtx_accessQueue.unlock();
                    if (s_Thr_PrepAndExec.head[4] == _PREPARE_STATEMENT) {
                        try {
                            memcpy(&ResponseToPrepare[2], &s_Thr_PrepAndExec.head[2], 2);
                            write(s_Thr_PrepAndExec.origin, ResponseToPrepare, sizeof(ResponseToPrepare));
                        }
                        catch (std::exception const& e) {
                            logs("PrepStatementResponse() : " + std::string(e.what()), ERROR);
                        }
                    }
                    else if (s_Thr_PrepAndExec.head[4] == _EXECUTE_STATEMENT) {
                        try {
                            // timestamp("Start Exec "+ idThread, std::chrono::high_resolution_clock::now());
                            memcpy(PreparedReqID, &s_Thr_PrepAndExec.CQLStatement[0], 18);
                            memcpy(tableName, &s_Thr_PrepAndExec.CQLStatement[27], 24);
                            for (tableNameSize = 0; tableNameSize < 24; tableNameSize++) {
                                if (tableName[tableNameSize] == 0x00) {
                                    break;
                                }
                            }
                            memset(createTable, 0x00, sizeof(createTable)); //Reset requests
                            memset(prepreq, 0x00, sizeof(prepreq));
                            memset(remPrepReq, 0x00, sizeof(remPrepReq));
                            memcpy(&createTable[0], &createTable_Req[0], sizeof(createTable_Req));    //COPY clauses
                            memcpy(&prepreq[0], &prepINSERT_Req[0], sizeof(prepINSERT_Req));
                            memcpy(&remPrepReq[0], &desalocateReq[0], sizeof(desalocateReq));
                            memcpy(&createTable[13], &tableName[0], tableNameSize);   //COPY table name
                            memcpy(&prepreq[12], &tableName[0], tableNameSize);
                            memcpy(&remPrepReq[11], &tableName[0], tableNameSize);
                            memcpy(&createTable[13 + tableNameSize], &createTableColumns_Req[0], sizeof(createTableColumns_Req));   //COPY columns name
                            memcpy(&prepreq[12 + tableNameSize], &prepINSERTColumns_Req[0], sizeof(prepINSERTColumns_Req));
                            // timestamp("PG comm start "+ idThread, std::chrono::high_resolution_clock::now());           //PERF OK
                            resCreateTable = PQexec(connPrepState, createTable);
                            if (PQresultStatus(resCreateTable) == PGRES_COMMAND_OK)
                            {
                                PQclear(resCreateTable);
                                // timestamp("CREATE TABLE time "+ idThread, std::chrono::high_resolution_clock::now());
                                // logsPGSQL(createTable);
                                prep = PQprepare(connPrepState, tableName, prepreq, 2, (const Oid)NULL); //TODO maybe tableName not good, to check and maybe delete prep requests after if needed
                                if (PQresultStatus(prep) == PGRES_COMMAND_OK) {
                                    // logsPGSQL(prepreq);
                                    cursor = tableNameSize + 27;
                                    fieldOrder = 0;
                                    while (s_Thr_PrepAndExec.CQLStatement[cursor] == 0x00 && s_Thr_PrepAndExec.CQLStatement[cursor + 1] == 0x00 && s_Thr_PrepAndExec.CQLStatement[cursor + 2] == 0x00 && s_Thr_PrepAndExec.CQLStatement[cursor + 3] == 0x64 && fieldOrder < 10) {
                                        memcpy(fieldDataExecute, &s_Thr_PrepAndExec.CQLStatement[cursor + 4], sizeof(fieldDataExecute));
                                        fieldColumnExecute[5] = stockPrepReq[fieldOrder];
                                        paramValues[0] = fieldColumnExecute;
                                        paramValues[1] = fieldDataExecute;
                                        fieldOrder++;
                                        prepExec = PQexecPrepared(connPrepState, tableName, 2, paramValues, paramLengths, paramFormats, 0);
                                        if (PQresultStatus(prepExec) == PGRES_COMMAND_OK) {
                                            if (fieldOrder == 9) {
                                                memcpy(&ResponseToExecute[2], &s_Thr_PrepAndExec.head[2], 2);
                                                write(s_Thr_PrepAndExec.origin, &ResponseToExecute, 13);
                                            }
                                        }
                                        else {
                                            // printf("HERE?\r\n");
                                            // logsPGSQL(PQresultErrorMessage(prepExec), ERROR);
                                            memcpy(&ResponseToExecute[2], &s_Thr_PrepAndExec.head[2], 2);
                                            write(s_Thr_PrepAndExec.origin, &ResponseToExecute, 13);
                                            break;
                                        }
                                        cursor += 104;
                                        PQclear(prepExec);
                                    }
                                }
                                else {
                                    logsPGSQL(PQresultErrorMessage(prep), ERROR);
                                }
                                PQclear(prep);
                                PQsendQuery(connPrepState, remPrepReq);
                            }
                            else {
                                logsPGSQL(createTable, ERROR);
                            }
                            memset(tableName, 0x00, sizeof(tableName));
                            // timestamp("Exec time "+ idThread, std::chrono::high_resolution_clock::now());
                        }
                        catch (std::exception const& e) {
                            logs("ExecutePrepStatement() : " + std::string(e.what()), ERROR);
                        }
                    }
                }
                else {
                    mtx_accessQueue.unlock();
                }
            }
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void PrepExecStatement(PGconn* connPrepState, PGconn* replic_connPrepState, server origin_server, server destination_server) {
    PGresult* prep;
    PGresult* prepExec;
    PGresult* resCreateTable;
    PGresult* resAlterPublication;
    PGresult* replic_resCreateTable;
    PGresult* replic_resAlterSubscription;
    PrepAndExecReq s_Thr_PrepAndExec;
    char* paramValues[2] = { "fieldx", "" };
    //char unsigned PreparedReqID[18];
    unsigned char ResponseToExecute_INSERT[13] = { 0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01 };
    //ADDED
    unsigned char ResponseToPrepare_SELECT[203] = {
        0x84, 0x00, 0x12, 0x40, 0x08, 0x00, 0x00, 0x00, 0xc2, 0x00, 0x00, 0x00, 0x04, 0x00, 0x10, 0xee,
        0xe3, 0x5a, 0x0a, 0xe7, 0x33, 0xd4, 0x3e, 0x68, 0xeb, 0x2b, 0xc4, 0xae, 0xa2, 0x3d, 0xed, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x79,
        0x63, 0x73, 0x62, 0x00, 0x09, 0x75, 0x73, 0x65, 0x72, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x04,
        0x79, 0x5f, 0x69, 0x64, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x04,
        0x79, 0x63, 0x73, 0x62, 0x00, 0x09, 0x75, 0x73, 0x65, 0x72, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x00,
        0x04, 0x79, 0x5f, 0x69, 0x64, 0x00, 0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x30, 0x00,
        0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x31, 0x00, 0x0d, 0x00, 0x06, 0x66, 0x69, 0x65,
        0x6c, 0x64, 0x32, 0x00, 0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x33, 0x00, 0x0d, 0x00,
        0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x34, 0x00, 0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64,
        0x35, 0x00, 0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x36, 0x00, 0x0d, 0x00, 0x06, 0x66,
        0x69, 0x65, 0x6c, 0x64, 0x37, 0x00, 0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x38, 0x00,
        0x0d, 0x00, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x39, 0x00, 0x0d
    };
    unsigned char ResponseToPrepare_UPDATE_0[88] = {
        0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x4f, 0x00, 0x00, 0x00, 0x04, 0x00, 0x10, 0x20,
        0x2a, 0x71, 0x57, 0x7b, 0xb2, 0xba, 0x33, 0x49, 0xb4, 0xb4, 0x66, 0x61, 0x52, 0xe2, 0x53, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x04, 0x79,
        0x63, 0x73, 0x62, 0x00, 0x09, 0x75, 0x73, 0x65, 0x72, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x06,
        0x66, 0x69, 0x65, 0x6c, 0x64, 0x30, 0x00, 0x0d, 0x00, 0x04, 0x79, 0x5f, 0x69, 0x64, 0x00, 0x0d,
        0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00
    };
    unsigned char ResponseToPrepare_UPDATE_1[88] = {
        0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x4f, 0x00, 0x00, 0x00, 0x04, 0x00, 0x10, 0x0a,
        0x50, 0x99, 0x62, 0xe3, 0x7f, 0xdb, 0x18, 0x3a, 0xe1, 0x21, 0xe9, 0xa6, 0x1c, 0xa3, 0x0b, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x04, 0x79,
        0x63, 0x73, 0x62, 0x00, 0x09, 0x75, 0x73, 0x65, 0x72, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x06,
        0x66, 0x69, 0x65, 0x6c, 0x64, 0x31, 0x00, 0x0d, 0x00, 0x04, 0x79, 0x5f, 0x69, 0x64, 0x00, 0x0d,
        0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00
    };
    unsigned char ResponseToPrepare_UPDATE_2[88] = {
        0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x4f, 0x00, 0x00, 0x00, 0x04, 0x00, 0x10, 0xf8,
        0xfc, 0x03, 0xa9, 0x4f, 0xcb, 0x71, 0xf8, 0x02, 0x12, 0xbe, 0x3b, 0x63, 0xe0, 0xd4, 0xca, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x04, 0x79,
        0x63, 0x73, 0x62, 0x00, 0x09, 0x75, 0x73, 0x65, 0x72, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x06,
        0x66, 0x69, 0x65, 0x6c, 0x64, 0x32, 0x00, 0x0d, 0x00, 0x04, 0x79, 0x5f, 0x69, 0x64, 0x00, 0x0d,
        0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00
    };
    unsigned char ResponseToPrepare_UPDATE_3[88] = {
        0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x4f, 0x00, 0x00, 0x00, 0x04, 0x00, 0x10, 0xbc,
        0x17, 0xfa, 0x30, 0xf8, 0xb9, 0xab, 0x5d, 0xb3, 0xb3, 0x02, 0x07, 0x8d, 0x36, 0x0f, 0xb7, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x04, 0x79,
        0x63, 0x73, 0x62, 0x00, 0x09, 0x75, 0x73, 0x65, 0x72, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x06,
        0x66, 0x69, 0x65, 0x6c, 0x64, 0x33, 0x00, 0x0d, 0x00, 0x04, 0x79, 0x5f, 0x69, 0x64, 0x00, 0x0d,
        0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00
    };
    unsigned char ResponseToPrepare_UPDATE_4[88] = {
        0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x4f, 0x00, 0x00, 0x00, 0x04, 0x00, 0x10, 0x27,
        0xdf, 0x6c, 0xde, 0x7b, 0x5b, 0x11, 0xe6, 0xc5, 0x81, 0xf1, 0xa7, 0x97, 0xf8, 0xf4, 0xf6, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x04, 0x79,
        0x63, 0x73, 0x62, 0x00, 0x09, 0x75, 0x73, 0x65, 0x72, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x06,
        0x66, 0x69, 0x65, 0x6c, 0x64, 0x34, 0x00, 0x0d, 0x00, 0x04, 0x79, 0x5f, 0x69, 0x64, 0x00, 0x0d,
        0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00
    };
    unsigned char ResponseToPrepare_UPDATE_5[88] = {
        0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x4f, 0x00, 0x00, 0x00, 0x04, 0x00, 0x10, 0x96,
        0x2c, 0xd3, 0xc1, 0x65, 0x95, 0x8a, 0xd7, 0x92, 0xc9, 0x2d, 0xb2, 0x0f, 0x7c, 0x58, 0xf5, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x04, 0x79,
        0x63, 0x73, 0x62, 0x00, 0x09, 0x75, 0x73, 0x65, 0x72, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x06,
        0x66, 0x69, 0x65, 0x6c, 0x64, 0x35, 0x00, 0x0d, 0x00, 0x04, 0x79, 0x5f, 0x69, 0x64, 0x00, 0x0d,
        0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00
    };
    unsigned char ResponseToPrepare_UPDATE_6[88] = {
        0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x4f, 0x00, 0x00, 0x00, 0x04, 0x00, 0x10, 0x93,
        0xed, 0xba, 0x0e, 0x1e, 0x92, 0xce, 0xf3, 0xe9, 0x87, 0xaa, 0xd4, 0x27, 0x02, 0x53, 0x49, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x04, 0x79,
        0x63, 0x73, 0x62, 0x00, 0x09, 0x75, 0x73, 0x65, 0x72, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x06,
        0x66, 0x69, 0x65, 0x6c, 0x64, 0x36, 0x00, 0x0d, 0x00, 0x04, 0x79, 0x5f, 0x69, 0x64, 0x00, 0x0d,
        0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00
    };
    unsigned char ResponseToPrepare_UPDATE_7[88] = {
        0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x4f, 0x00, 0x00, 0x00, 0x04, 0x00, 0x10, 0xa3,
        0x6c, 0x49, 0xa8, 0x66, 0x41, 0x9b, 0x02, 0xde, 0x8d, 0x52, 0x36, 0x0b, 0xfb, 0xa8, 0x7c, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x04, 0x79,
        0x63, 0x73, 0x62, 0x00, 0x09, 0x75, 0x73, 0x65, 0x72, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x06,
        0x66, 0x69, 0x65, 0x6c, 0x64, 0x37, 0x00, 0x0d, 0x00, 0x04, 0x79, 0x5f, 0x69, 0x64, 0x00, 0x0d,
        0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00
    };
    unsigned char ResponseToPrepare_UPDATE_8[88] = {
        0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x4f, 0x00, 0x00, 0x00, 0x04, 0x00, 0x10, 0x03,
        0x2c, 0x1c, 0xc3, 0x18, 0xb2, 0x34, 0xca, 0xff, 0xae, 0xea, 0xa5, 0xe3, 0xb2, 0x6e, 0x55, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x04, 0x79,
        0x63, 0x73, 0x62, 0x00, 0x09, 0x75, 0x73, 0x65, 0x72, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x06,
        0x66, 0x69, 0x65, 0x6c, 0x64, 0x38, 0x00, 0x0d, 0x00, 0x04, 0x79, 0x5f, 0x69, 0x64, 0x00, 0x0d,
        0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00
    };
    unsigned char ResponseToPrepare_UPDATE_9[88] = {
        0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x4f, 0x00, 0x00, 0x00, 0x04, 0x00, 0x10, 0x22,
        0xfe, 0x26, 0x1e, 0xda, 0x92, 0x8e, 0x3e, 0x0a, 0x98, 0xad, 0xc0, 0x6a, 0x28, 0x84, 0x75, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x04, 0x79,
        0x63, 0x73, 0x62, 0x00, 0x09, 0x75, 0x73, 0x65, 0x72, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x06,
        0x66, 0x69, 0x65, 0x6c, 0x64, 0x39, 0x00, 0x0d, 0x00, 0x04, 0x79, 0x5f, 0x69, 0x64, 0x00, 0x0d,
        0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00
    };
    //ENDADDED
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
    char createTable[85];
    char prepreq[91];
    char remPrepReq[100];
    char fieldColumnExecute[] = "fieldx";
    char fieldDataExecute[100];
    char tableName[24];
    int cursor = 0;
    int fieldOrder = 0;

    //(Longueurs estimées par rapport à ce qu'ils allaient contenir)
    //int size = 0;     //...
    /*char createPublication[100];
    char createSubscription[175];*/
    char alterPublication[90];
    char alterSubscription[100];
    // std::ostringstream ss;
    // ss <<  std::this_thread::get_id();
    // std::string idThread = ss.str();
    while (bl_continueThread) {
        if (q_PrepAndExecRequests.size() > 0) {
            if (mtx_accessQueue.try_lock()) {
                if (q_PrepAndExecRequests.size() > 0) {
                    printf("%zu\r\n", q_PrepAndExecRequests.size());
                    memcpy(&s_Thr_PrepAndExec.head[0], &q_PrepAndExecRequests.front().head[0], sizeof(s_Thr_PrepAndExec.head));
                    memcpy(&s_Thr_PrepAndExec.CQLStatement[0], &q_PrepAndExecRequests.front().CQLStatement[0], sizeof(s_Thr_PrepAndExec.CQLStatement));
                    s_Thr_PrepAndExec.origin = q_PrepAndExecRequests.front().origin;
                    /*std::cout << "_PrepAndExecReq.head: " << std::endl;
                    for(int i = 0; i < sizeof(s_Thr_PrepAndExec.head); i++)
                        std::cout << q_PrepAndExecRequests.front().head[i];
                    std::cout << " _PrepAndExecReq.CQLStatement: " << std::endl;
                    for(int i = 0; i < sizeof(s_Thr_PrepAndExec.CQLStatement); i++)
                        std::cout <<  q_PrepAndExecRequests.front().CQLStatement[i];*/
                    q_PrepAndExecRequests.pop();
                    mtx_accessQueue.unlock();
                    if (s_Thr_PrepAndExec.head[4] == _PREPARE_STATEMENT) {
                        try {
                            std::cout << "== _PREPARE_STATEMENT" << std::endl;
                            //ADDED
                            switch (s_Thr_PrepAndExec.CQLStatement[0])
                            {
                            case (0x49):      //_I_NSERT
                                memcpy(&ResponseToPrepare_INSERT[2], &s_Thr_PrepAndExec.head[2], 2);
                                write(s_Thr_PrepAndExec.origin, ResponseToPrepare_INSERT, sizeof(ResponseToPrepare_INSERT));
                                std::cout << "WRITED _PREPARE_STATEMENT_INSERT" << std::endl;
                                break;

                            case (0x53):      //_S_ELECT
                                memcpy(&ResponseToPrepare_SELECT[2], &s_Thr_PrepAndExec.head[2], 2);
                                write(s_Thr_PrepAndExec.origin, ResponseToPrepare_SELECT, sizeof(ResponseToPrepare_SELECT));
                                std::cout << "WRITED _PREPARE_STATEMENT_SELECT" << std::endl;
                                break;

                            case (0x55):      //_U_PDATE
                                switch (s_Thr_PrepAndExec.CQLStatement[26])
                                {
                                case (0x30):      //field_0_
                                    memcpy(&ResponseToPrepare_UPDATE_0[2], &s_Thr_PrepAndExec.head[2], 2);
                                    write(s_Thr_PrepAndExec.origin, ResponseToPrepare_UPDATE_0, sizeof(ResponseToPrepare_UPDATE_0));
                                    std::cout << "WRITED _PREPARE_STATEMENT_UPDATE_0" << std::endl;
                                    break;

                                case (0x31):      //field_1_
                                    memcpy(&ResponseToPrepare_UPDATE_1[2], &s_Thr_PrepAndExec.head[2], 2);
                                    write(s_Thr_PrepAndExec.origin, ResponseToPrepare_UPDATE_1, sizeof(ResponseToPrepare_UPDATE_1));
                                    std::cout << "WRITED _PREPARE_STATEMENT_UPDATE_1" << std::endl;
                                    break;

                                case (0x32):      //field_2_
                                    memcpy(&ResponseToPrepare_UPDATE_2[2], &s_Thr_PrepAndExec.head[2], 2);
                                    write(s_Thr_PrepAndExec.origin, ResponseToPrepare_UPDATE_2, sizeof(ResponseToPrepare_UPDATE_2));
                                    std::cout << "WRITED _PREPARE_STATEMENT_UPDATE_2" << std::endl;
                                    break;

                                case (0x33):      //field_3_
                                    memcpy(&ResponseToPrepare_UPDATE_3[2], &s_Thr_PrepAndExec.head[2], 2);
                                    write(s_Thr_PrepAndExec.origin, ResponseToPrepare_UPDATE_3, sizeof(ResponseToPrepare_UPDATE_3));
                                    std::cout << "WRITED _PREPARE_STATEMENT_UPDATE_3" << std::endl;
                                    break;

                                case (0x34):      //field_4_
                                    memcpy(&ResponseToPrepare_UPDATE_4[2], &s_Thr_PrepAndExec.head[2], 2);
                                    write(s_Thr_PrepAndExec.origin, ResponseToPrepare_UPDATE_4, sizeof(ResponseToPrepare_UPDATE_4));
                                    std::cout << "WRITED _PREPARE_STATEMENT_UPDATE_4" << std::endl;
                                    break;

                                case (0x35):      //field_5_
                                    memcpy(&ResponseToPrepare_UPDATE_5[2], &s_Thr_PrepAndExec.head[2], 2);
                                    write(s_Thr_PrepAndExec.origin, ResponseToPrepare_UPDATE_5, sizeof(ResponseToPrepare_UPDATE_5));
                                    std::cout << "WRITED _PREPARE_STATEMENT_UPDATE_5" << std::endl;
                                    break;

                                case (0x36):      //field_6_
                                    memcpy(&ResponseToPrepare_UPDATE_6[2], &s_Thr_PrepAndExec.head[2], 2);
                                    write(s_Thr_PrepAndExec.origin, ResponseToPrepare_UPDATE_6, sizeof(ResponseToPrepare_UPDATE_6));
                                    std::cout << "WRITED _PREPARE_STATEMENT_UPDATE_6" << std::endl;
                                    break;

                                case (0x37):      //field_7_
                                    memcpy(&ResponseToPrepare_UPDATE_7[2], &s_Thr_PrepAndExec.head[2], 2);
                                    write(s_Thr_PrepAndExec.origin, ResponseToPrepare_UPDATE_7, sizeof(ResponseToPrepare_UPDATE_7));
                                    std::cout << "WRITED _PREPARE_STATEMENT_UPDATE_7" << std::endl;
                                    break;

                                case (0x38):      //field_8_
                                    memcpy(&ResponseToPrepare_UPDATE_8[2], &s_Thr_PrepAndExec.head[2], 2);
                                    write(s_Thr_PrepAndExec.origin, ResponseToPrepare_UPDATE_8, sizeof(ResponseToPrepare_UPDATE_8));
                                    std::cout << "WRITED _PREPARE_STATEMENT_UPDATE_8" << std::endl;
                                    break;

                                case (0x39):      //field_9_
                                    memcpy(&ResponseToPrepare_UPDATE_9[2], &s_Thr_PrepAndExec.head[2], 2);
                                    write(s_Thr_PrepAndExec.origin, ResponseToPrepare_UPDATE_9, sizeof(ResponseToPrepare_UPDATE_9));
                                    std::cout << "WRITED _PREPARE_STATEMENT_UPDATE_9" << std::endl;
                                    break;

                                default:
                                    std::cout << "WRITED _PREPARE_STATEMENT_UPDATE_ELSE" << std::endl;
                                }

                            default:
                                std::cout << "WRITED _PREPARE_STATEMENT_ELSE" << std::endl;
                            }
                            //ENDADDED                          
                        }
                        catch (std::exception const& e) {
                            logs("PrepStatementResponse() : " + std::string(e.what()), ERROR);
                        }
                    }
                    else if (s_Thr_PrepAndExec.head[4] == _EXECUTE_STATEMENT) {
                        try {
                            std::cout << "== _EXECUTE_STATEMENT" << std::endl;
                            // timestamp("Start Exec "+ idThread, std::chrono::high_resolution_clock::now());
                            /*memcpy(PreparedReqID, &s_Thr_PrepAndExec.CQLStatement[0], 18);
                            std::cout << "memcpy PreparedReqID: " << std::endl;
                            for(int i = 0; i<18;i++)
                                std::cout << PreparedReqID[i];*/

                                //ADDED
                            char arg_list[10][100];
                            char temp_arg[100];
                            int size = 0;
                            int argNumber = 0;
                            cursor = 19;

                            while (s_Thr_PrepAndExec.CQLStatement[cursor] == 0x00 && s_Thr_PrepAndExec.CQLStatement[cursor + 1] == 0x00 && s_Thr_PrepAndExec.CQLStatement[cursor + 2] != 0x13 && s_Thr_PrepAndExec.CQLStatement[cursor + 3] != 0x88 && argNumber < 10)
                            {
                                size = (unsigned int)s_Thr_PrepAndExec.CQLStatement[cursor + 2] * 256 + (unsigned int)s_Thr_PrepAndExec.CQLStatement[cursor + 3];
                                if (size == sizeof(temp_arg))    //Value
                                {
                                    memset(temp_arg, 0x00, sizeof(temp_arg));
                                    memcpy(&temp_arg, &s_Thr_PrepAndExec.CQLStatement[cursor + 4], sizeof(temp_arg));
                                    memcpy(&arg_list[argNumber], &temp_arg, sizeof(temp_arg));
                                    argNumber++;
                                }
                                else        //Table name
                                {
                                    tableNameSize = size;
                                    memcpy(tableName, &s_Thr_PrepAndExec.CQLStatement[cursor + 4], tableNameSize);
                                }
                                cursor += (size + 4);
                            }

                            switch (argNumber) {
                            case 0:     //SELECT
                                std::cout << "C'est un select, on gère ça plus tard..." << std::endl;

                                break;
                            case 1:     //UPDATE
                                std::cout << "C'est un update, on gère ça plus tard..." << std::endl;
                                break;
                            case 10:    //INSERT
                                std::cout << "C'est un insert..." << std::endl;
                                memset(createTable, 0x00, sizeof(createTable)); //Reset requests
                                memset(prepreq, 0x00, sizeof(prepreq));
                                memset(remPrepReq, 0x00, sizeof(remPrepReq));
                                memcpy(&createTable[0], &createTable_Req[0], sizeof(createTable_Req));    //COPY clauses
                                memcpy(&prepreq[0], &prepINSERT_Req[0], sizeof(prepINSERT_Req));
                                memcpy(&remPrepReq[0], &desalocateReq[0], sizeof(desalocateReq));
                                memcpy(&createTable[13], &tableName[0], tableNameSize);   //COPY table name
                                memcpy(&prepreq[12], &tableName[0], tableNameSize);
                                memcpy(&remPrepReq[11], &tableName[0], tableNameSize);
                                memcpy(&createTable[13 + tableNameSize], &createTableColumns_Req[0], sizeof(createTableColumns_Req));   //COPY columns name
                                memcpy(&prepreq[12 + tableNameSize], &prepINSERTColumns_Req[0], sizeof(prepINSERTColumns_Req));
                                // timestamp("PG comm start "+ idThread, std::chrono::high_resolution_clock::now());           //PERF OK

                                //CREATE TABLE on publisher
                                resCreateTable = PQexec(connPrepState, createTable);
                                //ALTER PUBLICATION
                                /*size = 0;
                                memset(alterPublication, 0x00, sizeof(alterPublication));

                                memcpy(&alterPublication[size],&alterPublication_Req[0],sizeof(alterPublication_Req)-1);
                                size += sizeof(alterPublication_Req)-1;
                                memcpy(&alterPublication[size],&(origin_server.server_name.c_str())[0],origin_server.server_name.length());
                                size += origin_server.server_name.length();
                                memcpy(&alterPublication[size],&alterPublicationAdd_Req[0],sizeof(alterPublicationAdd_Req)-1);
                                size += sizeof(alterPublicationAdd_Req)-1;
                                memcpy(&alterPublication[size],&tableName[0],tableNameSize);
                                size += tableNameSize;
                                alterPublication[size] = ';';
                                //std::cout << "STATUS: " << PQresultStatus(resCreateTable) << std::endl;

                                resAlterPublication = PQexec(connPrepState, alterPublication);
                                PQclear(resAlterPublication);

                                //CREATE TABLE on subscriber
                                replic_resCreateTable = PQexec(replic_connPrepState, createTable);      //Execute la même requête de création de table sur le subscriber
                                PQclear(replic_resCreateTable);
                                //ALTER SUBSCRIPTION
                                size = 0;
                                memset(alterSubscription, 0x00, sizeof(alterSubscription));

                                memcpy(&alterSubscription[size],&alterSubscription_Req[0],sizeof(alterSubscription_Req)-1);
                                size += sizeof(alterSubscription_Req)-1;
                                memcpy(&alterSubscription[size],&(destination_server.server_name.c_str())[0],destination_server.server_name.length());
                                size += destination_server.server_name.length();
                                memcpy(&alterSubscription[size],&alterSubscriptionRefresh_Req[0],sizeof(alterSubscriptionRefresh_Req)-1);
                                //std::cout << "STATUS: " << PQresultStatus(replic_resCreateTable) << std::endl;

                                replic_resAlterSubscription = PQexec(replic_connPrepState, alterSubscription);
                                PQclear(replic_resAlterSubscription);*/

                                if (PQresultStatus(resCreateTable) == PGRES_COMMAND_OK)
                                {
                                    PQclear(resCreateTable);

                                    // timestamp("CREATE TABLE time "+ idThread, std::chrono::high_resolution_clock::now());
                                    // logsPGSQL(createTable);
                                    prep = PQprepare(connPrepState, tableName, prepreq, 2, (const Oid)NULL); //TODO maybe tableName not good, to check and maybe delete prep requests after if needed
                                    if (PQresultStatus(prep) == PGRES_COMMAND_OK) {
                                        std::cout << "PQprepare: == PGRES_COMMAND_OK" << std::endl;
                                        // logsPGSQL(prepreq);
                                        for (int i = 0; i < argNumber; i++)
                                        {
                                            memcpy(fieldDataExecute, &s_Thr_PrepAndExec.CQLStatement[cursor + 4], sizeof(fieldDataExecute));
                                            fieldColumnExecute[5] = stockPrepReq[i];
                                            paramValues[0] = fieldColumnExecute;
                                            paramValues[1] = arg_list[i];
                                            prepExec = PQexecPrepared(connPrepState, tableName, 2, paramValues, paramLengths, paramFormats, 0);
                                            if (PQresultStatus(prepExec) == PGRES_COMMAND_OK) {
                                                if (i == 9) {
                                                    memcpy(&ResponseToExecute_INSERT[2], &s_Thr_PrepAndExec.head[2], 2);
                                                    write(s_Thr_PrepAndExec.origin, &ResponseToExecute_INSERT, 13);
                                                    std::cout << "PQexecPrepared: == PGRES_COMMAND_OK" << std::endl;
                                                }
                                            }
                                            else {
                                                // printf("HERE?\r\n");
                                                // logsPGSQL(PQresultErrorMessage(prepExec), ERROR);
                                                memcpy(&ResponseToExecute_INSERT[2], &s_Thr_PrepAndExec.head[2], 2);
                                                write(s_Thr_PrepAndExec.origin, &ResponseToExecute_INSERT, 13);
                                                std::cout << "PQexecPrepared: != PGRES_COMMAND_OK" << std::endl;
                                                break;
                                            }
                                            PQclear(prepExec);
                                        }
                                    }
                                    else {
                                        std::cout << "PQprepare: != PGRES_COMMAND_OK" << std::endl;
                                        logsPGSQL(PQresultErrorMessage(prep), ERROR);
                                    }
                                    PQclear(prep);
                                    PQsendQuery(connPrepState, remPrepReq);
                                }
                                else {
                                    logsPGSQL(createTable, ERROR);
                                }
                                memset(tableName, 0x00, sizeof(tableName));
                                break;

                            default:
                                std::cout << "Je sais pas ce que c'est..." << std::endl;
                                break;
                            }
                            //ENDADDED
                        }
                        catch (std::exception const& e) {
                            logs("ExecutePrepStatement() : " + std::string(e.what()), ERROR);
                        }
                    }
                }
                else {
                    mtx_accessQueue.unlock();
                }
            }
        }
        else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}
//#pragma endregion Functions
