#include "c_PreparedStatements.hpp"
#include "c_Socket.hpp"

#define _NPOS std::string::npos
#define _PREPARE_STATEMENT 0x09
#define _EXECUTE_STATEMENT 0x0a

std::queue<PrepAndExecReq> q_PrepAndExecRequests;
std::mutex mtx_accessQueue;
std::mutex mtx_writeOrder;
std::mutex mtx_writeClient;
std::mutex mtx_getID;

bool bl_continueThread = true;
int ID_Thread=0;

void* ConnPGSQLPrepStatements(void* arg) {    //Create the threads that will read the prepared statements requests
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
        logs("ConnPGSQLPrepStatements() : Secure search path error", ERROR);
    else
        logs("ConnPGSQLPrepStatements() : Connexion to PostgreSQL sucess");
    PQclear(resPrepState);
    printf("Yoloooooo\r\n");
    PrepExecStatement(connPrepState, arg);

    //At the end, closing
    PQfinish(connPrepState);
    logs("ConnPGSQLPrepStatements() : Fermeture du thread");
    delete arg;
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

void PrepExecStatement(PGconn *connPrepState, void* arg){
    PGresult *resCreateTable;
    const char* command;
    char defaultINSERT[] = "CALL insert_th0($1::varchar(24),$2::char(100),$3::char(100),$4::char(100),$5::char(100),$6::char(100),$7::char(100),$8::char(100),$9::char(100),$10::char(100),$11::char(100));";
    const char * paramValues[11];
    //unsigned char stockPrepReq[10] = {0x31, 0x30, 0x37, 0x36, 0x39, 0x38, 0x33, 0x32, 0x35, 0x34};
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
    char fieldDataExecute[10][100];
    char tableName[24];
    int cursor = 0;
    PrepAndExecReq s_Thr_PrepAndExec;

    const char connection_string_host[] = "host = ";      //...destination_server.server_ip_address
    const char connection_string_user[] = " user = postgres password = Gang-bang69";
    PGconn* replic_connPrepState;
    PGresult* replic_resPrepState;
    replication_relation* replication_servers;
    server destination_server;
    char replic_conninfoPrepState[65];
    int id_thr;
    bool bl_repl = false;
    while(!mtx_getID.try_lock()){}
    id_thr = ID_Thread;
    ID_Thread++;
    mtx_getID.unlock();

    //Not optimal but called once
    const char* idTh;
    idTh = (std::to_string(id_thr)).c_str();
    memcpy(&defaultINSERT[14], idTh, 1);
    command = defaultINSERT;
    std::cout<<std::string(command)<<std::endl;
    if(arg != NULL){
        replication_servers = (replication_relation*)arg;
        // origin_server = replication_servers->publisher;
        id_thr = replication_servers->th_num;
        //std::cout << "Serveur origine: " << origin_server.server_id << " | " << origin_server.server_ip_address << " | " << origin_server.server_name << std::endl;
        destination_server = replication_servers->subscriber;
        //std::cout << "Serveur destination: " << destination_server.server_id << " | " << destination_server.server_ip_address << " | " << destination_server.server_name << std::endl;

        memcpy(&replic_conninfoPrepState[0], &connection_string_host[0], sizeof(connection_string_host) - 1);
        memcpy(&replic_conninfoPrepState[sizeof(connection_string_host) - 1], &(destination_server.server_ip_address.c_str())[0], destination_server.server_ip_address.length());
        memcpy(&replic_conninfoPrepState[sizeof(connection_string_host) - 1 + destination_server.server_ip_address.length()], &connection_string_user[0], sizeof(connection_string_user) - 1);

        replic_connPrepState = PQconnectdb(replic_conninfoPrepState);
        /* Check to see that the backend connection was successfully made */
        if (PQstatus(replic_connPrepState) != CONNECTION_OK)
        {
            logs("ConnPGSQLPrepStatements() : Connexion to database failed", ERROR);
        }
        /* Set always-secure search path, so malicious users can't take control. */
        replic_resPrepState = PQexec(replic_connPrepState, "SELECT pg_catalog.set_config('search_path', 'public', false)");
        if (PQresultStatus(replic_resPrepState) != PGRES_TUPLES_OK)
            logs("ConnPGSQLPrepStatements() : Secure search path error", ERROR);
        else
        {
            bl_repl=true;
            logs("ConnPGSQLPrepStatements() : Connexion to PostgreSQL sucess");
        }
        PQclear(replic_resPrepState);
    }

    while(bl_continueThread){
        if(q_PrepAndExecRequests.size()>0){
            if(mtx_accessQueue.try_lock()){
                if(q_PrepAndExecRequests.size()>0){
                    memcpy(&s_Thr_PrepAndExec.head[0], &q_PrepAndExecRequests.front().head[0], sizeof(s_Thr_PrepAndExec.head));
                    memcpy(&s_Thr_PrepAndExec.CQLStatement[0], &q_PrepAndExecRequests.front().CQLStatement[0], sizeof(s_Thr_PrepAndExec.CQLStatement));
                    s_Thr_PrepAndExec.origin = q_PrepAndExecRequests.front().origin;
                    q_PrepAndExecRequests.pop();
                    mtx_accessQueue.unlock();
                    if(s_Thr_PrepAndExec.head[4] == _PREPARE_STATEMENT){
                        memcpy(&ResponseToPrepInsert[2], &s_Thr_PrepAndExec.head[2], 2);
                        write(s_Thr_PrepAndExec.origin, ResponseToPrepInsert, sizeof(ResponseToPrepInsert));
                    }
                    else if(s_Thr_PrepAndExec.head[4] == _EXECUTE_STATEMENT){
                            memcpy(&tableName[0], &s_Thr_PrepAndExec.CQLStatement[27], 24);
                            for(tableNameSize=0; tableNameSize < 24; tableNameSize++){
                                if(tableName[tableNameSize] == 0x00)
                                    break;
                            }
                            cursor = tableNameSize+27;
                            paramValues[0] =  tableName;
                            for(int i=0; i<10; i++){
                                memcpy(&fieldDataExecute[i], &s_Thr_PrepAndExec.CQLStatement[cursor+4], sizeof(fieldDataExecute[i])+2);
                                paramValues[i+1] = fieldDataExecute[i];
                                cursor += 104;
                            }
                            resCreateTable = PQexecParams(connPrepState, command, 11, (const Oid*) NULL, paramValues, NULL, NULL, 0);
                            if (PQresultStatus(resCreateTable) == PGRES_COMMAND_OK)
                            {
                                PQclear(resCreateTable);
                                memcpy(&ResponseToExecute[2], &s_Thr_PrepAndExec.head[2], 2);
                                write(s_Thr_PrepAndExec.origin, &ResponseToExecute, 13);
                                for(int i=0; i<10; i++)
                                    memset(&fieldDataExecute[i], 0x00, sizeof(fieldDataExecute[i]));
                            }
                            else
                            {
                                fprintf(stderr, "LISTEN command failed: %s", PQerrorMessage(connPrepState));
                                PQclear(resCreateTable);
                            }
                            memset(tableName, 0x00, sizeof(tableName));
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
    PQfinish(replic_connPrepState);

}