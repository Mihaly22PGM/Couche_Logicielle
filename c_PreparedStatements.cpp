#include "c_PreparedStatements.hpp"
#include "c_Socket.hpp"

// #define _NPOS std::string::npos
#define _PREPARE_STATEMENT 0x09
#define _EXECUTE_STATEMENT 0x0a
#define _ITER_FOR_PUB_REFRESH 50

std::queue<PrepAndExecReq> q_PrepAndExecRequests;
std::mutex mtx_accessQueue;
// std::mutex mtx_writeOrder;
// std::mutex mtx_writeClient;
std::mutex mtx_getID;

bool bl_continueThread = true;
int ID_Thread = 0;

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
    //ADDED
    PGresult* resCreateTable_repl;
    //ENDADDED
    const char* command;
    const char* command_sub;
    char defaultINSERT[] = "CALL pub_insert($1::varchar(24),$2::char(100),$3::char(100),$4::char(100),$5::char(100),$6::char(100),$7::char(100),$8::char(100),$9::char(100),$10::char(100),$11::char(100));";
    char defaultINSERTsub[] = "CALL sub_insert_sx($1::varchar(24));";
    char defaultREFRESHpub[] = "ALTER SUBSCRIPTION s_sx REFRESH PUBLICATION;";
    const char* paramValues[11];
    //ADDED
    const char* paramValues_repl[1];

    //REPLICATION FACTOR

    //ENDADDED
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

    //ADDED
    //POUR LE RUN
    /* unsigned char ResponseToPrepare_SELECT[203] = {
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
    };*/
    //ENDADDED

    int tableNameSize = 0;
    char fieldDataExecute[10][100];
    char tableName[24];
    int cursor = 0;
    PrepAndExecReq s_Thr_PrepAndExec;

    const char connection_string_host[] = "host = ";      //...destination_server.server_ip_address
    const char connection_string_user[] = " user = postgres password = Gang-bang69";    //TODO is password usefull?
    PGconn* replic_connPrepState;
    PGresult* replic_resPrepState;
    replication_relation* replication_servers;
    //ADDED
    server origin_server;
    //ENDADDED
    server destination_server;

    //REPLICATION FACTOR

    char replic_conninfoPrepState[65];

    //REPLICATION FACTOR

    int id_thr;
    int iter_refreshPub = 0;
    bool bl_firstThread = false;
    // bool bl_repl = false;

    //Set Thread ID
    while (!mtx_getID.try_lock()) {}
    id_thr = ID_Thread;
    ID_Thread++;
    mtx_getID.unlock();
    if (id_thr == 0)
        bl_firstThread = true;

    //Set Thread ID into requests
    // const char* idTh;
    // idTh = (std::to_string(id_thr)).c_str();
    // memcpy(&defaultINSERT[14], idTh, 1);
    command = defaultINSERT;

    if (arg != NULL) {
        replication_servers = (replication_relation*)arg;
        //ADDED
        origin_server = replication_servers->publisher;
        //ENDADDED
        // id_thr = replication_servers->th_num;
        destination_server = replication_servers->subscriber;

        memcpy(&replic_conninfoPrepState[0], &connection_string_host[0], sizeof(connection_string_host) - 1);
        memcpy(&replic_conninfoPrepState[sizeof(connection_string_host) - 1], &(destination_server.server_ip_address.c_str())[0], destination_server.server_ip_address.length());
        memcpy(&replic_conninfoPrepState[sizeof(connection_string_host) - 1 + destination_server.server_ip_address.length()], &connection_string_user[0], sizeof(connection_string_user) - 1);

        //MOVED
        // memcpy(&defaultINSERTsub[21], idTh, 1);
        //ADDED
        memcpy(&defaultINSERTsub[17], (std::to_string(origin_server.server_id)).c_str(), 1);
        memcpy(&defaultREFRESHpub[22], (std::to_string(origin_server.server_id)).c_str(), 1);
        //ENDADDED
        command_sub = defaultINSERTsub;
        //ENDMOVED

        replic_connPrepState = PQconnectdb(replic_conninfoPrepState);
        /* Check to see that the backend connection was successfully made */
        if (PQstatus(replic_connPrepState) != CONNECTION_OK)
        {
            logs("ConnPGSQLPrepStatements() REPLIC: Connexion to database failed", ERROR);
        }
        /* Set always-secure search path, so malicious users can't take control. */
        replic_resPrepState = PQexec(replic_connPrepState, "SELECT pg_catalog.set_config('search_path', 'public', false)");
        if (PQresultStatus(replic_resPrepState) != PGRES_TUPLES_OK)
            logs("ConnPGSQLPrepStatements() REPLIC: Secure search path error", ERROR);
        else
        {
            // bl_repl=true;
            logs("ConnPGSQLPrepStatements() REPLIC: Connexion to PostgreSQL sucess");
        }
        PQclear(replic_resPrepState);

        //REPLICATION FACTOR

    }

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
                        //std::cout << "== _PREPARE_STATEMENT" << std::endl;
                        memcpy(&ResponseToPrepare_INSERT[2], &s_Thr_PrepAndExec.head[2], 2);
                        write(s_Thr_PrepAndExec.origin, ResponseToPrepare_INSERT, sizeof(ResponseToPrepare_INSERT));
                        //std::cout << "WRITED _PREPARE_STATEMENT_INSERT" << std::endl;
                    }
                    else if (s_Thr_PrepAndExec.head[4] == _EXECUTE_STATEMENT) {
                        //std::cout << "== _EXECUTE_STATEMENT" << std::endl;
                        memcpy(&tableName[0], &s_Thr_PrepAndExec.CQLStatement[27], 24);
                        for (tableNameSize = 0; tableNameSize < 24; tableNameSize++) {
                            if (tableName[tableNameSize] == 0x00)
                                break;
                        }
                        cursor = tableNameSize + 27;
                        paramValues[0] = tableName;
                        //ADDED
                        paramValues_repl[0] = tableName;
                        //std::cout << "tableName: " << std::string(tableName) << std::endl;
                        //ENDADDED
                        for (int i = 0; i < 10; i++) {
                            memcpy(&fieldDataExecute[i], &s_Thr_PrepAndExec.CQLStatement[cursor + 4], sizeof(fieldDataExecute[i]) + 2);
                            paramValues[i + 1] = fieldDataExecute[i];
                            // std::cout << "paramValues[i+1]: " << std::string(paramValues[i + 1]) << std::endl;
                            cursor += 104;
                        }
                        //std::cout << std::string(command) << std::endl;
                        resCreateTable = PQexecParams(connPrepState, command, 11, (const Oid*)NULL, paramValues, NULL, NULL, 0);
                        if (PQresultStatus(resCreateTable) == PGRES_COMMAND_OK)
                        {
                            PQclear(resCreateTable);
                            //ADDED
                            if (arg != NULL)
                            {
                                //REPLICATION FACTOR

                                // std::cout << std::string(command_sub) << std::endl;
                                resCreateTable_repl = PQexecParams(replic_connPrepState, command_sub, 1, (const Oid*)NULL, paramValues_repl, NULL, NULL, 0);
                                if (PQresultStatus(resCreateTable_repl) != PGRES_COMMAND_OK)
                                    fprintf(stderr, "LISTEN command REPLIC failed: %s", PQerrorMessage(replic_connPrepState));
                                PQclear(resCreateTable_repl);
                            }
                            //ENDADDED
                            //std::cout << "WRITED _EXECUTE_STATEMENT_INSERT" << std::endl;

                        }
                        else
                        {
                            fprintf(stderr, "LISTEN command failed: %s", PQerrorMessage(connPrepState));
                            PQclear(resCreateTable);
                        }
                        //MOVED
                        memcpy(&ResponseToExecute_INSERT[2], &s_Thr_PrepAndExec.head[2], 2);
                        write(s_Thr_PrepAndExec.origin, &ResponseToExecute_INSERT, 13);
                        for (int i = 0; i < 10; i++)
                            memset(&fieldDataExecute[i], 0x00, sizeof(fieldDataExecute[i]));
                        //ENDMOVED
                        memset(tableName, 0x00, sizeof(tableName));
                    }
                    if (bl_firstThread) {
                        iter_refreshPub++;
                        if (iter_refreshPub > _ITER_FOR_PUB_REFRESH) {
                            while (!mtx_accessQueue.try_lock()) {}
                            resCreateTable = PQexec(replic_connPrepState, defaultREFRESHpub);
                            if (PQresultStatus(resCreateTable) == PGRES_COMMAND_OK)
                                iter_refreshPub = 0;
                            else
                                fprintf(stderr, "REFRESH command REPLIC failed: %s", PQerrorMessage(replic_connPrepState));
                            PQclear(resCreateTable);
                            mtx_accessQueue.unlock();
                        }
                    }
                }
                else
                    mtx_accessQueue.unlock();
            }
        }
        else
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    PQfinish(replic_connPrepState);
}