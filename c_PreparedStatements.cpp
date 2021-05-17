#include "c_PreparedStatements.hpp"
#include "c_Socket.hpp"

#define _PREPARE_STATEMENT 0x09
#define _EXECUTE_STATEMENT 0x0a
#define _RETRYING 10

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

    const char* command_INSERT;
    char defaultINSERT[] = "CALL pub_insert($1::varchar(24),$2::char(100),$3::char(100),$4::char(100),$5::char(100),$6::char(100),$7::char(100),$8::char(100),$9::char(100),$10::char(100),$11::char(100));";
    const char* paramValues_INSERT[11];

    // const char* command_SELECT;
    // char defaultSELECT[] = "CALL pub_select($1::varchar(24));";
    char select_req[] = "SELECT * FROM                         ;";
    // const char* paramValues_SELECT[1];

    unsigned char full_text[15000];

    const unsigned char response[] = { 0x84, 0x00, 'x', 'x', 0x08, 0x00, 0x00, 'y', 'y', 0x00, 0x00, 0x00, 0x02, 0x00,
                                        0x00, 0x00, 0x05, 0x00, 0x00, 'z', 'z', 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00 };
    const unsigned char perm_double_zero[] = { 0x00, 0x00 };
    const unsigned char perm_null_element[] = { 0xff, 0xff, 0xff, 0xff };

    const char* command_UPDATE;
    char defaultUPDATE[] = "CALL pub_update($1::varchar(24),$2::char(6),$3::char(100));";
    const char* paramValues_UPDATE[3];

    //INSERT
    unsigned char ResponseToPrepare_INSERT[] = {
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
    unsigned char ResponseToExecute_INSERT[] = {
        0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01
    };

    //SELECT
    unsigned char ResponseToPrepare_SELECT[] = {
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

    //UPDATE
    unsigned char ResponseToPrepare_UPDATE_0[] = {
        0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x4f, 0x00, 0x00, 0x00, 0x04, 0x00, 0x10, 0x20,
        0x2a, 0x71, 0x57, 0x7b, 0xb2, 0xba, 0x33, 0x49, 0xb4, 0xb4, 0x66, 0x61, 0x52, 0xe2, 0x53, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x04, 0x79,
        0x63, 0x73, 0x62, 0x00, 0x09, 0x75, 0x73, 0x65, 0x72, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x06,
        0x66, 0x69, 0x65, 0x6c, 0x64, 0x30, 0x00, 0x0d, 0x00, 0x04, 0x79, 0x5f, 0x69, 0x64, 0x00, 0x0d,
        0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00
    };

    unsigned char ResponseToPrepare_UPDATE_1[] = {
        0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x4f, 0x00, 0x00, 0x00, 0x04, 0x00, 0x10, 0x0a,
        0x50, 0x99, 0x62, 0xe3, 0x7f, 0xdb, 0x18, 0x3a, 0xe1, 0x21, 0xe9, 0xa6, 0x1c, 0xa3, 0x0b, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x04, 0x79,
        0x63, 0x73, 0x62, 0x00, 0x09, 0x75, 0x73, 0x65, 0x72, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x06,
        0x66, 0x69, 0x65, 0x6c, 0x64, 0x31, 0x00, 0x0d, 0x00, 0x04, 0x79, 0x5f, 0x69, 0x64, 0x00, 0x0d,
        0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00
    };

    unsigned char ResponseToPrepare_UPDATE_2[] = {
        0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x4f, 0x00, 0x00, 0x00, 0x04, 0x00, 0x10, 0xf8,
        0xfc, 0x03, 0xa9, 0x4f, 0xcb, 0x71, 0xf8, 0x02, 0x12, 0xbe, 0x3b, 0x63, 0xe0, 0xd4, 0xca, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x04, 0x79,
        0x63, 0x73, 0x62, 0x00, 0x09, 0x75, 0x73, 0x65, 0x72, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x06,
        0x66, 0x69, 0x65, 0x6c, 0x64, 0x32, 0x00, 0x0d, 0x00, 0x04, 0x79, 0x5f, 0x69, 0x64, 0x00, 0x0d,
        0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00
    };

    unsigned char ResponseToPrepare_UPDATE_3[] = {
        0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x4f, 0x00, 0x00, 0x00, 0x04, 0x00, 0x10, 0xbc,
        0x17, 0xfa, 0x30, 0xf8, 0xb9, 0xab, 0x5d, 0xb3, 0xb3, 0x02, 0x07, 0x8d, 0x36, 0x0f, 0xb7, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x04, 0x79,
        0x63, 0x73, 0x62, 0x00, 0x09, 0x75, 0x73, 0x65, 0x72, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x06,
        0x66, 0x69, 0x65, 0x6c, 0x64, 0x33, 0x00, 0x0d, 0x00, 0x04, 0x79, 0x5f, 0x69, 0x64, 0x00, 0x0d,
        0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00
    };

    unsigned char ResponseToPrepare_UPDATE_4[] = {
        0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x4f, 0x00, 0x00, 0x00, 0x04, 0x00, 0x10, 0x27,
        0xdf, 0x6c, 0xde, 0x7b, 0x5b, 0x11, 0xe6, 0xc5, 0x81, 0xf1, 0xa7, 0x97, 0xf8, 0xf4, 0xf6, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x04, 0x79,
        0x63, 0x73, 0x62, 0x00, 0x09, 0x75, 0x73, 0x65, 0x72, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x06,
        0x66, 0x69, 0x65, 0x6c, 0x64, 0x34, 0x00, 0x0d, 0x00, 0x04, 0x79, 0x5f, 0x69, 0x64, 0x00, 0x0d,
        0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00
    };

    unsigned char ResponseToPrepare_UPDATE_5[] = {
        0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x4f, 0x00, 0x00, 0x00, 0x04, 0x00, 0x10, 0x96,
        0x2c, 0xd3, 0xc1, 0x65, 0x95, 0x8a, 0xd7, 0x92, 0xc9, 0x2d, 0xb2, 0x0f, 0x7c, 0x58, 0xf5, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x04, 0x79,
        0x63, 0x73, 0x62, 0x00, 0x09, 0x75, 0x73, 0x65, 0x72, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x06,
        0x66, 0x69, 0x65, 0x6c, 0x64, 0x35, 0x00, 0x0d, 0x00, 0x04, 0x79, 0x5f, 0x69, 0x64, 0x00, 0x0d,
        0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00
    };

    unsigned char ResponseToPrepare_UPDATE_6[] = {
        0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x4f, 0x00, 0x00, 0x00, 0x04, 0x00, 0x10, 0x93,
        0xed, 0xba, 0x0e, 0x1e, 0x92, 0xce, 0xf3, 0xe9, 0x87, 0xaa, 0xd4, 0x27, 0x02, 0x53, 0x49, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x04, 0x79,
        0x63, 0x73, 0x62, 0x00, 0x09, 0x75, 0x73, 0x65, 0x72, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x06,
        0x66, 0x69, 0x65, 0x6c, 0x64, 0x36, 0x00, 0x0d, 0x00, 0x04, 0x79, 0x5f, 0x69, 0x64, 0x00, 0x0d,
        0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00
    };

    unsigned char ResponseToPrepare_UPDATE_7[] = {
        0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x4f, 0x00, 0x00, 0x00, 0x04, 0x00, 0x10, 0xa3,
        0x6c, 0x49, 0xa8, 0x66, 0x41, 0x9b, 0x02, 0xde, 0x8d, 0x52, 0x36, 0x0b, 0xfb, 0xa8, 0x7c, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x04, 0x79,
        0x63, 0x73, 0x62, 0x00, 0x09, 0x75, 0x73, 0x65, 0x72, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x06,
        0x66, 0x69, 0x65, 0x6c, 0x64, 0x37, 0x00, 0x0d, 0x00, 0x04, 0x79, 0x5f, 0x69, 0x64, 0x00, 0x0d,
        0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00
    };

    unsigned char ResponseToPrepare_UPDATE_8[] = {
        0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x4f, 0x00, 0x00, 0x00, 0x04, 0x00, 0x10, 0x03,
        0x2c, 0x1c, 0xc3, 0x18, 0xb2, 0x34, 0xca, 0xff, 0xae, 0xea, 0xa5, 0xe3, 0xb2, 0x6e, 0x55, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x04, 0x79,
        0x63, 0x73, 0x62, 0x00, 0x09, 0x75, 0x73, 0x65, 0x72, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x06,
        0x66, 0x69, 0x65, 0x6c, 0x64, 0x38, 0x00, 0x0d, 0x00, 0x04, 0x79, 0x5f, 0x69, 0x64, 0x00, 0x0d,
        0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00
    };

    unsigned char ResponseToPrepare_UPDATE_9[] = {
        0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x4f, 0x00, 0x00, 0x00, 0x04, 0x00, 0x10, 0x22,
        0xfe, 0x26, 0x1e, 0xda, 0x92, 0x8e, 0x3e, 0x0a, 0x98, 0xad, 0xc0, 0x6a, 0x28, 0x84, 0x75, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x04, 0x79,
        0x63, 0x73, 0x62, 0x00, 0x09, 0x75, 0x73, 0x65, 0x72, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x06,
        0x66, 0x69, 0x65, 0x6c, 0x64, 0x39, 0x00, 0x0d, 0x00, 0x04, 0x79, 0x5f, 0x69, 0x64, 0x00, 0x0d,
        0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00
    };

    unsigned char ResponseToExecute_UPDATE[] = {
        0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01
    };

    int tableNameSize = 0;
    char fieldDataExecute[10][100];
    char tableName[24];
    int cursor = 0;
    PrepAndExecReq s_Thr_PrepAndExec;

    command_INSERT = defaultINSERT;
    // command_SELECT = defaultSELECT;
    command_UPDATE = defaultUPDATE;

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
                        switch (s_Thr_PrepAndExec.CQLStatement[0])
                        {
                        case (0x49):      //_I_NSERT
                            memcpy(&ResponseToPrepare_INSERT[2], &s_Thr_PrepAndExec.head[2], 2);
                            write(s_Thr_PrepAndExec.origin, ResponseToPrepare_INSERT, sizeof(ResponseToPrepare_INSERT));
                            break;

                        case (0x53):      //_S_ELECT
                            memcpy(&ResponseToPrepare_SELECT[2], &s_Thr_PrepAndExec.head[2], 2);
                            write(s_Thr_PrepAndExec.origin, ResponseToPrepare_SELECT, sizeof(ResponseToPrepare_SELECT));
                            break;

                        case (0x55):      //_U_PDATE
                            switch (s_Thr_PrepAndExec.CQLStatement[26])
                            {
                            case (0x30):      //field_0_
                                memcpy(&ResponseToPrepare_UPDATE_0[2], &s_Thr_PrepAndExec.head[2], 2);
                                write(s_Thr_PrepAndExec.origin, ResponseToPrepare_UPDATE_0, sizeof(ResponseToPrepare_UPDATE_0));
                                break;

                            case (0x31):      //field_1_
                                memcpy(&ResponseToPrepare_UPDATE_1[2], &s_Thr_PrepAndExec.head[2], 2);
                                write(s_Thr_PrepAndExec.origin, ResponseToPrepare_UPDATE_1, sizeof(ResponseToPrepare_UPDATE_1));
                                break;

                            case (0x32):      //field_2_
                                memcpy(&ResponseToPrepare_UPDATE_2[2], &s_Thr_PrepAndExec.head[2], 2);
                                write(s_Thr_PrepAndExec.origin, ResponseToPrepare_UPDATE_2, sizeof(ResponseToPrepare_UPDATE_2));
                                break;

                            case (0x33):      //field_3_
                                memcpy(&ResponseToPrepare_UPDATE_3[2], &s_Thr_PrepAndExec.head[2], 2);
                                write(s_Thr_PrepAndExec.origin, ResponseToPrepare_UPDATE_3, sizeof(ResponseToPrepare_UPDATE_3));
                                break;

                            case (0x34):      //field_4_
                                memcpy(&ResponseToPrepare_UPDATE_4[2], &s_Thr_PrepAndExec.head[2], 2);
                                write(s_Thr_PrepAndExec.origin, ResponseToPrepare_UPDATE_4, sizeof(ResponseToPrepare_UPDATE_4));
                                break;

                            case (0x35):      //field_5_
                                memcpy(&ResponseToPrepare_UPDATE_5[2], &s_Thr_PrepAndExec.head[2], 2);
                                write(s_Thr_PrepAndExec.origin, ResponseToPrepare_UPDATE_5, sizeof(ResponseToPrepare_UPDATE_5));
                                break;

                            case (0x36):      //field_6_
                                memcpy(&ResponseToPrepare_UPDATE_6[2], &s_Thr_PrepAndExec.head[2], 2);
                                write(s_Thr_PrepAndExec.origin, ResponseToPrepare_UPDATE_6, sizeof(ResponseToPrepare_UPDATE_6));
                                break;

                            case (0x37):      //field_7_
                                memcpy(&ResponseToPrepare_UPDATE_7[2], &s_Thr_PrepAndExec.head[2], 2);
                                write(s_Thr_PrepAndExec.origin, ResponseToPrepare_UPDATE_7, sizeof(ResponseToPrepare_UPDATE_7));
                                break;

                            case (0x38):      //field_8_
                                memcpy(&ResponseToPrepare_UPDATE_8[2], &s_Thr_PrepAndExec.head[2], 2);
                                write(s_Thr_PrepAndExec.origin, ResponseToPrepare_UPDATE_8, sizeof(ResponseToPrepare_UPDATE_8));
                                break;

                            case (0x39):      //field_9_
                                memcpy(&ResponseToPrepare_UPDATE_9[2], &s_Thr_PrepAndExec.head[2], 2);
                                write(s_Thr_PrepAndExec.origin, ResponseToPrepare_UPDATE_9, sizeof(ResponseToPrepare_UPDATE_9));
                                break;

                            default:
                                break;
                            }
                            break;

                        default:
                            break;
                        }
                    }
                    else if (s_Thr_PrepAndExec.head[4] == _EXECUTE_STATEMENT) {
                        //SELECT
                        if (s_Thr_PrepAndExec.head[8] == 0x3e || s_Thr_PrepAndExec.head[8] == 0x3d || s_Thr_PrepAndExec.head[8] == 0x3c || s_Thr_PrepAndExec.head[8] == 0x3b || s_Thr_PrepAndExec.head[8] == 0x3a || s_Thr_PrepAndExec.head[8] == 0x39 || s_Thr_PrepAndExec.head[8] == 0x38)
                        {
                            memcpy(&tableName[0], &s_Thr_PrepAndExec.CQLStatement[27], 24);
                            for (tableNameSize = 0; tableNameSize < 24; tableNameSize++) {
                                if (tableName[tableNameSize] == 0x00 || (unsigned int)(tableName[tableNameSize]) == 0xffffffff)
                                    break;
                            }

                            // paramValues_SELECT[0] = tableName;
                            memcpy(&select_req[14], &tableName, tableNameSize);

                            //resCreateTable = PQexecParams(connPrepState, command_SELECT, 1, (const Oid*)NULL, paramValues_SELECT, NULL, NULL, 0);
                            resCreateTable = PQexec(connPrepState, select_req);

                            for (int i = 0; i < _RETRYING; i++)
                            {
                                if (PQresultStatus(resCreateTable) == PGRES_TUPLES_OK)
                                {
                                    cursor = 0;
                                    memcpy(&full_text[cursor], &response, sizeof(response));
                                    cursor += sizeof(response);

                                    full_text[19] = 0x00;
                                    full_text[20] = 0x0b;
                                    full_text[27] = tableNameSize / 256;
                                    full_text[28] = tableNameSize;
                                    cursor += 1;
                                    memcpy(&full_text[cursor], &tableName, tableNameSize);
                                    cursor += tableNameSize;

                                    for (int i = 0; i < PQntuples(resCreateTable); i++)     //VALEURS
                                    {
                                        if (PQgetvalue(resCreateTable, i, 1) != NULL)
                                        {
                                            memcpy(&full_text[cursor], &perm_double_zero, 2);
                                            cursor += 2;
                                            full_text[cursor] = strlen(PQgetvalue(resCreateTable, i, 1)) / 256;
                                            full_text[cursor + 1] = strlen(PQgetvalue(resCreateTable, i, 1));
                                            cursor += 2;
                                            memcpy(&full_text[cursor], PQgetvalue(resCreateTable, i, 1), strlen(PQgetvalue(resCreateTable, i, 1)));
                                            cursor += strlen(PQgetvalue(resCreateTable, i, 1));
                                        }
                                        else
                                        {
                                            memcpy(&full_text[cursor], &perm_null_element, 4);
                                            cursor += 4;
                                        }
                                    }
                                    full_text[7] = (cursor - 9) / 256;
                                    full_text[8] = cursor - 9;

                                    memcpy(&full_text[2], &s_Thr_PrepAndExec.head[2], 2);
                                    write(s_Thr_PrepAndExec.origin, full_text, cursor);
                                    //PQclear(resCreateTable);
                                    break;
                                }
                                else
                                {
                                    PQclear(resCreateTable);
                                    resCreateTable = PQexec(connPrepState, select_req);
                                    // std::cout << "RETRY #" << i << std::endl;
                                }
                                if (i == _RETRYING - 1)
                                {
                                    logs(PQerrorMessage(connPrepState), ERROR);
                                    std::cout << "!!! ECHEC SELECT !!! table: " << tableName << std::endl;
                                }
                            }
                            PQclear(resCreateTable);
                            /*if (PQresultStatus(resCreateTable) == PGRES_TUPLES_OK)
                            {
                                cursor = 0;
                                memcpy(&full_text[cursor], &response, sizeof(response));
                                cursor += sizeof(response);

                                full_text[19] = 0x00;
                                full_text[20] = 0x0b;
                                full_text[27] = tableNameSize / 256;
                                full_text[28] = tableNameSize;
                                cursor += 1;
                                memcpy(&full_text[cursor], &tableName, tableNameSize);
                                cursor += tableNameSize;

                                for (int i = 0; i < PQntuples(resCreateTable); i++)     //VALEURS
                                {
                                    if (PQgetvalue(resCreateTable, i, 1) != NULL)
                                    {
                                        memcpy(&full_text[cursor], &perm_double_zero, 2);
                                        cursor += 2;
                                        full_text[cursor] = strlen(PQgetvalue(resCreateTable, i, 1)) / 256;
                                        full_text[cursor + 1] = strlen(PQgetvalue(resCreateTable, i, 1));
                                        cursor += 2;
                                        memcpy(&full_text[cursor], PQgetvalue(resCreateTable, i, 1), strlen(PQgetvalue(resCreateTable, i, 1)));
                                        cursor += strlen(PQgetvalue(resCreateTable, i, 1));
                                    }
                                    else
                                    {
                                        memcpy(&full_text[cursor], &perm_null_element, 4);
                                        cursor += 4;
                                    }
                                }
                                full_text[7] = (cursor - 9) / 256;
                                full_text[8] = cursor - 9;

                                memcpy(&full_text[2] , &s_Thr_PrepAndExec.head[2], 2);
                                write(s_Thr_PrepAndExec.origin, full_text, cursor);
                                PQclear(resCreateTable);
                            }
                            else
                            {
                                fprintf(stderr, "SELECT statement failed: %s", PQerrorMessage(connPrepState));
                                PQclear(resCreateTable);
                            }*/

                            for (int i = 0; i < 10; i++)
                                memset(&fieldDataExecute[i], 0x00, sizeof(fieldDataExecute[i]));
                            memset(&select_req[14], 0x00, tableNameSize);
                            memset(tableName, 0x00, sizeof(tableName));
                            memset(full_text, 0x00, sizeof(full_text));
                        }

                        //UPDATE    
                        else if (s_Thr_PrepAndExec.head[8] == 0xa6 || s_Thr_PrepAndExec.head[8] == 0xa5 || s_Thr_PrepAndExec.head[8] == 0xa4 || s_Thr_PrepAndExec.head[8] == 0xa3 || s_Thr_PrepAndExec.head[8] == 0xa2 || s_Thr_PrepAndExec.head[8] == 0xa1)
                        {
                            memcpy(&tableName[0], &s_Thr_PrepAndExec.CQLStatement[131], 24);
                            for (tableNameSize = 0; tableNameSize < 24; tableNameSize++) {
                                if (tableName[tableNameSize] == 0x00)
                                    break;
                            }
                            paramValues_UPDATE[0] = tableName;

                            switch (s_Thr_PrepAndExec.CQLStatement[2])
                            {
                            case(0x20):
                                paramValues_UPDATE[1] = "field0";
                                break;

                            case(0x0a):
                                paramValues_UPDATE[1] = "field1";
                                break;

                            case(0xf8):
                                paramValues_UPDATE[1] = "field2";
                                break;

                            case(0xbc):
                                paramValues_UPDATE[1] = "field3";
                                break;

                            case(0x27):
                                paramValues_UPDATE[1] = "field4";
                                break;

                            case(0x96):
                                paramValues_UPDATE[1] = "field5";
                                break;

                            case(0x93):
                                paramValues_UPDATE[1] = "field6";
                                break;

                            case(0xa3):
                                paramValues_UPDATE[1] = "field7";
                                break;

                            case(0x03):
                                paramValues_UPDATE[1] = "field8";
                                break;

                            case(0x22):
                                paramValues_UPDATE[1] = "field9";
                                break;

                            default:
                                std::cout << " Unknown EXECUTE_UPDATE field " << std::endl;
                                break;
                            }

                            memcpy(&fieldDataExecute[0], &s_Thr_PrepAndExec.CQLStatement[27], sizeof(fieldDataExecute[0]));
                            paramValues_UPDATE[2] = fieldDataExecute[0];

                            resCreateTable = PQexecParams(connPrepState, command_UPDATE, 3, (const Oid*)NULL, paramValues_UPDATE, NULL, NULL, 0);

                            for (int i = 0; i < _RETRYING; i++)
                            {
                                if ((PQresultStatus(resCreateTable) == PGRES_COMMAND_OK) || ((PQresultStatus(resCreateTable) == PGRES_FATAL_ERROR) && (std::string(PQerrorMessage(connPrepState)).substr(0, 16) == "ERROR:  relation")))
                                {
                                    memcpy(&ResponseToExecute_UPDATE[2], &s_Thr_PrepAndExec.head[2], 2);
                                    write(s_Thr_PrepAndExec.origin, &ResponseToExecute_UPDATE, 13);
                                    break;
                                }
                                else
                                {
                                    PQclear(resCreateTable);
                                    resCreateTable = PQexecParams(connPrepState, command_UPDATE, 3, (const Oid*)NULL, paramValues_UPDATE, NULL, NULL, 0);
                                    // std::cout << "RETRY #" << i << std::endl;
                                }
                                if (i == _RETRYING - 1)
                                {
                                    logs(PQerrorMessage(connPrepState), ERROR);
                                    std::cout << "!!! ECHEC UPDATE !!! table: " << tableName << std::endl;
                                }
                            }
                            PQclear(resCreateTable);

                            /*if (PQresultStatus(resCreateTable) == PGRES_COMMAND_OK)
                            {
                                memcpy(&ResponseToExecute_UPDATE[2], &s_Thr_PrepAndExec.head[2], 2);
                                write(s_Thr_PrepAndExec.origin, &ResponseToExecute_UPDATE, 13);
                                PQclear(resCreateTable);
                            }
                            else
                            {
                                fprintf(stderr, "UPDATE statement failed: %s", PQerrorMessage(connPrepState));
                                PQclear(resCreateTable);
                            }*/

                            for (int i = 0; i < 10; i++)
                                memset(&fieldDataExecute[i], 0x00, sizeof(fieldDataExecute[i]));
                            memset(tableName, 0x00, sizeof(tableName));
                        }

                        //INSERT
                        else if (s_Thr_PrepAndExec.head[7] == 0x04 || s_Thr_PrepAndExec.head[8] == 0x66 || s_Thr_PrepAndExec.head[8] == 0x65 || s_Thr_PrepAndExec.head[8] == 0x64 || s_Thr_PrepAndExec.head[8] == 0x63 || s_Thr_PrepAndExec.head[8] == 0x62)
                        {
                            memcpy(&tableName[0], &s_Thr_PrepAndExec.CQLStatement[27], 24);
                            for (tableNameSize = 0; tableNameSize < 24; tableNameSize++) {
                                if (tableName[tableNameSize] == 0x00)
                                    break;
                            }
                            cursor = tableNameSize + 27;
                            paramValues_INSERT[0] = tableName;

                            for (int i = 0; i < 10; i++) {
                                memcpy(&fieldDataExecute[i], &s_Thr_PrepAndExec.CQLStatement[cursor + 4], sizeof(fieldDataExecute[i]) + 2);
                                paramValues_INSERT[i + 1] = fieldDataExecute[i];
                                cursor += 104;
                            }

                            resCreateTable = PQexecParams(connPrepState, command_INSERT, 11, (const Oid*)NULL, paramValues_INSERT, NULL, NULL, 0);

                            for (int i = 0; i < _RETRYING; i++)
                            {
                                if ((PQresultStatus(resCreateTable) == PGRES_COMMAND_OK) || ((PQresultStatus(resCreateTable) == PGRES_FATAL_ERROR) && (std::string(PQerrorMessage(connPrepState)).substr(0, 16) == "ERROR:  relation")))
                                {
                                    memcpy(&ResponseToExecute_INSERT[2], &s_Thr_PrepAndExec.head[2], 2);
                                    write(s_Thr_PrepAndExec.origin, &ResponseToExecute_INSERT, 13);
                                    break;
                                }
                                else
                                {
                                    PQclear(resCreateTable);
                                    resCreateTable = PQexecParams(connPrepState, command_INSERT, 11, (const Oid*)NULL, paramValues_INSERT, NULL, NULL, 0);
                                    // std::cout << "RETRY #" << i << std::endl;
                                }
                                if (i == _RETRYING - 1)
                                {
                                    logs(PQerrorMessage(connPrepState), ERROR);
                                    std::cout << "!!! ECHEC INSERT !!! table: " << tableName << std::endl;
                                }
                            }
                            PQclear(resCreateTable);

                            /*if (PQresultStatus(resCreateTable) == PGRES_COMMAND_OK)
                            {
                                memcpy(&ResponseToExecute_INSERT[2], &s_Thr_PrepAndExec.head[2], 2);
                                write(s_Thr_PrepAndExec.origin, &ResponseToExecute_INSERT, 13);
                                PQclear(resCreateTable);
                            }
                            else
                            {
                                fprintf(stderr, "INSERT statement failed: %s", PQerrorMessage(connPrepState));
                                PQclear(resCreateTable);
                            }*/

                            for (int i = 0; i < 10; i++)
                                memset(&fieldDataExecute[i], 0x00, sizeof(fieldDataExecute[i]));
                            memset(tableName, 0x00, sizeof(tableName));
                        }

                        else
                        {
                            std::cout << "EXECUTE_ELSE" << std::endl;
                            printf("s_Thr_PrepAndExec.head[8]: 0x%x", s_Thr_PrepAndExec.head[8]);
                        }
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