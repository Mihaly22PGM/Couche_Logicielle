#include "c_PreparedStatements.hpp"
#include "c_Socket.hpp"

#define _PREPARE_STATEMENT 0x09
#define _EXECUTE_STATEMENT 0x0a

//#pragma region Global
std::queue<PrepAndExecReq> q_PrepAndExecRequests;
std::mutex mtx_accessQueue;
std::mutex mtx_writeOrder;
std::mutex mtx_writeClient;
bool bl_continueThread = true;

//MOVED
const char* conninfoPrepState = "user = postgres";
const char connection_string_host[] = "host = ";      //...destination_server.server_ip_address
const char connection_string_user[] = " user = postgres password = Gang-bang69";

const char createTable_Req[] = "CREATE TABLE ";
const char createTableColumns_Req[] = " (column_name varchar(255), value varchar(255));";
const char prepINSERT_Req[] = "INSERT INTO ";
const char prepINSERTColumns_Req[] = " (column_name, value) VALUES ($1::varchar,$2::varchar);";
const char desalocateReq[] = "DEALLOCATE ";

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
//ENDMOVED
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

    //ADDED
    if (arg != NULL)
    {
        replication_relation* replication_servers = (replication_relation*)arg;
        server origin_server = replication_servers->publisher;
        //std::cout << "Serveur origine: " << origin_server.server_id << " | " << origin_server.server_ip_address << " | " << origin_server.server_name << std::endl;
        server destination_server = replication_servers->subscriber;
        //std::cout << "Serveur destination: " << destination_server.server_id << " | " << destination_server.server_ip_address << " | " << destination_server.server_name << std::endl;

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
        //ENDADDED
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
                    q_PrepAndExecRequests.pop();
                    mtx_accessQueue.unlock();
                    if (s_Thr_PrepAndExec.head[4] == _PREPARE_STATEMENT) {
                        try {
                            memcpy(&ResponseToPrepare[2], &s_Thr_PrepAndExec.head[2], 2);
                            write(GetSocket(), ResponseToPrepare, sizeof(ResponseToPrepare));
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
                                                //CHANGED
                                                write(s_Thr_PrepAndExec.origin, &ResponseToExecute, 13);
                                                //ENDCHANGED
                                            }
                                        }
                                        else {
                                            // printf("HERE?\r\n");
                                            // logsPGSQL(PQresultErrorMessage(prepExec), ERROR);
                                            memcpy(&ResponseToExecute[2], &s_Thr_PrepAndExec.head[2], 2);
                                            //CHANGED
                                            write(s_Thr_PrepAndExec.origin, &ResponseToExecute, 13);
                                            //ENDCHANGED
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

//ADDED
void PrepExecStatement(PGconn* connPrepState, PGconn* replic_connPrepState, server origin_server, server destination_server) {
    PGresult* prep;
    PGresult* prepExec;
    PGresult* resCreateTable;
    //ADDED
    PGresult* resAlterPublication;
    PGresult* replic_resCreateTable;
    PGresult* replic_resAlterSubscription;
    //ENDADDED
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

    //ADDED
    //(Longueurs estim�es par rapport � ce qu'ils allaient contenir)
    int size = 0;
    /*char createPublication[100];
    char createSubscription[175];*/
    char alterPublication[90];
    char alterSubscription[100];
    //ENDADDED
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
                    q_PrepAndExecRequests.pop();
                    mtx_accessQueue.unlock();
                    if (s_Thr_PrepAndExec.head[4] == _PREPARE_STATEMENT) {
                        try {
                            memcpy(&ResponseToPrepare[2], &s_Thr_PrepAndExec.head[2], 2);
                            write(GetSocket(), ResponseToPrepare, sizeof(ResponseToPrepare));
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

                            //CREATE TABLE on publisher
                            resCreateTable = PQexec(connPrepState, createTable);
                            //ADDED
                            //ALTER PUBLICATION
                            size = 0;
                            memset(alterPublication, 0x00, sizeof(alterPublication));

                            memcpy(&alterPublication[size], &alterPublication_Req[0], sizeof(alterPublication_Req) - 1);
                            size += sizeof(alterPublication_Req) - 1;
                            memcpy(&alterPublication[size], &(origin_server.server_name.c_str())[0], origin_server.server_name.length());
                            size += origin_server.server_name.length();
                            memcpy(&alterPublication[size], &alterPublicationAdd_Req[0], sizeof(alterPublicationAdd_Req) - 1);
                            size += sizeof(alterPublicationAdd_Req) - 1;
                            memcpy(&alterPublication[size], &tableName[0], tableNameSize);
                            size += tableNameSize;
                            alterPublication[size] = ';';
                            //std::cout << "STATUS: " << PQresultStatus(resCreateTable) << std::endl;

                            resAlterPublication = PQexec(connPrepState, alterPublication);

                            PQclear(resAlterPublication);
                            /*std::cout << "alterPublication done" << std::endl;
                            for(int i = 0; i < sizeof(alterPublication); i++)
                                std::cout << alterPublication[i];*/

                                //CREATE TABLE on subscriber
                            replic_resCreateTable = PQexec(replic_connPrepState, createTable);      //Execute la m�me requ�te de cr�ation de table sur le subscriber
                            PQclear(replic_resCreateTable);
                            //ALTER SUBSCRIPTION
                            size = 0;
                            memset(alterSubscription, 0x00, sizeof(alterSubscription));

                            memcpy(&alterSubscription[size], &alterSubscription_Req[0], sizeof(alterSubscription_Req) - 1);
                            size += sizeof(alterSubscription_Req) - 1;
                            memcpy(&alterSubscription[size], &(destination_server.server_name.c_str())[0], destination_server.server_name.length());
                            size += destination_server.server_name.length();
                            memcpy(&alterSubscription[size], &alterSubscriptionRefresh_Req[0], sizeof(alterSubscriptionRefresh_Req) - 1);
                            //std::cout << "STATUS: " << PQresultStatus(replic_resCreateTable) << std::endl;

                            replic_resAlterSubscription = PQexec(replic_connPrepState, alterSubscription);
                            PQclear(replic_resAlterSubscription);
                            /*std::cout << "alterSubscription done" << std::endl;
                            for(int i = 0; i < sizeof(alterSubscription); i++)
                                std::cout << alterSubscription[i];*/
                                //ENDADDED
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
                                                //CHANGED
                                                write(s_Thr_PrepAndExec.origin, &ResponseToExecute, 13);
                                                //ENDCHANGED
                                            }
                                        }
                                        else {
                                            // printf("HERE?\r\n");
                                            // logsPGSQL(PQresultErrorMessage(prepExec), ERROR);
                                            memcpy(&ResponseToExecute[2], &s_Thr_PrepAndExec.head[2], 2);
                                            //CHANGED
                                            write(s_Thr_PrepAndExec.origin, &ResponseToExecute, 13);
                                            //ENDCHANGED
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
//ENDADDED
//#pragma endregion Functions
