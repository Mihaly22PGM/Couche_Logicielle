#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <pthread.h>
#include <list>
#include <vector>
#include <queue>
#include <mutex>
#include <string.h>
#include <algorithm>
#include <future>
#include "c_Socket.hpp"
#include "c_Logs.hpp"
#include "c_PreparedStatements.hpp"
#include "libpq-fe.h"
#include <fcntl.h>

//Debug libraries
#include <iostream>
#include <sys/stat.h>
#include <time.h>
#include <chrono>
#include <fstream>
#include <functional>       
#include <ifaddrs.h>

using namespace std;
using std::string;

#define _NPOS std::string::npos
#define _OPTIONS_STATEMENT 0x05
#define _QUERY_STATEMENT 0x07
#define _PREPARE_STATEMENT 0x09
#define _EXECUTE_STATEMENT 0x0a
#define _THREDS_EXEC_NUMBER 6
typedef int SOCKET;

#pragma region DeleteForProd
string const nomFichier("/home/tfe_rwcs/Couche_Logicielle/Request.log");
const std::string& fileExistRun = "run";
struct stat buffe;
// string const fileExistRun("run");
ofstream fileToRun(fileExistRun.c_str());
ofstream fichier(nomFichier.c_str());
#pragma endregion DeleteForProd

#pragma region Structures

struct Requests {
    char opcode[1];
    char stream[2];
    int size;
    unsigned char request[2048];
    int origin; //sockDataClient = issu du serveur, accepted_connections[i] = issu de la redirection
};
struct SQLRequests {
    char stream[2];
    char key_name[255];
    int pos_key;
    char key[255];
    string request;
    int origin; //sockDataClient = issu du serveur, accepted_connections[i] = issu de la redirection
};

struct char_array {
    char chararray[65536];
};

#pragma endregion Structures

#pragma region Global
bool bl_UseReplication = false;
bool bl_UseBench = false;
bool bl_Load = false;
bool bl_loop = true;
const char* conninfo = "user = postgres";
char buffData[65536];

std::queue<char_array> l_bufferFrames;
std::list<Requests> l_bufferRequests;
std::list<SQLRequests> l_bufferPGSQLRequests;
size_t from_sub_pos, where_sub_pos, limit_sub_pos, set_sub_pos, values_sub_pos = 0;
std::string select_clause, from_clause, where_clause, update_clause, set_clause, insert_into_clause, values_clause, delete_clause = "";
std::string table, key = "";
int pos_key = 0;
std::string key_name = "";
std::string LowerRequest = "";
std::string _incoming_cql_query = "";
vector<std::string> fields, values, columns;

SOCKET sockServer;
SOCKET sockDataClient;
unsigned char UseResponse[] = { 0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x03, 0x00, 0x04, 0x79, 0x63, 0x73, 0x62 };

int socket_for_client, client_connection;

unsigned char perm_double_zero[2] = { 0x00, 0x00 };
unsigned char perm_column_separator[2] = { 0x00, 0x0d };
unsigned char perm_column_value_separation[4] = { 0x00, 0x00, 0x00, 0x01 };
unsigned char perm_null_element[4] = { 0xff, 0xff, 0xff, 0xff };

//threads
pthread_t th_FrameClient;
pthread_t th_FrameData;
pthread_t th_Requests;
pthread_t th_PostgreSQL;
pthread_t th_Redirecting;
pthread_t th_INITSocket_Redirection;
pthread_t th_PrepExec[_THREDS_EXEC_NUMBER];
std::mutex mtx_q_frames;
pthread_t th_Listening_socket;

PGconn* conn;
PGresult* res;

server actual_server;
server subscriber_server;
server neighbor_server_1;
server server_to_redirect;
int port = 8042;
std::list<Requests> l_bufferRequestsForActualServer;
std::list<Requests> l_bufferPreparedReq;
int socket_neighbor_1;
vector<int> accepted_connections, connected_connections;

//Liste des serveurs
/*server server_A = { "RWCS-vServer1", 0, "192.168.82.55" };
server server_B = { "RWCS-vServer2", 1, "192.168.82.61" };
server server_C = { "RWCS-vServer3", 2, "192.168.82.63" };
server server_D = { "RWCS-vServer4", 3, "192.168.82.56" };
server server_E = { "RWCS-vServer5", 4, "192.168.82.58" };
server server_F = { "RWCS-vServer6", 5, "192.168.82.59" };*/
server server_A = { "RWCS_vServer4", 0, "192.168.82.56" };
server server_B = { "RWCS_vServer5", 1, "192.168.82.58" };
server server_C = { "RWCS_vServer3", 2, "192.168.82.64" };
server server_D = { "RWCS_vServer4", 3, "192.168.82.56" };
server server_E = { "RWCS_vServer5", 4, "192.168.82.58" };
server server_F = { "RWCS_vServer6", 5, "192.168.82.59" };
//Important de respecter l'ordre des id quand on déclare les sevreurs dans la liste pour que ça coincide avec la position dans la liste
vector<server> l_servers = { server_A/*, server_B, server_C, server_D, server_E, server_F*/ };
const int server_count = l_servers.size();

#pragma endregion Global

#pragma region Prototypes
void* TraitementFrameData(void*);
void* TraitementRequests(void*);
void* SendPGSQL(void*);
void* INITSocket_Redirection(void*);
void* redirecting(void*);
void CQLtoSQL(SQLRequests);
void ConnexionPGSQL();
void closeSockets();
void closeThreads();
void exit_prog(int);
void send_to_server(int, char[2], string);
void server_identification();
void create_select_sql_query(string, string, vector<string>, char[2], string, int, int);
void create_update_sql_query(string, string, vector<string>, char[2], int);
void create_insert_sql_query(string, string, vector<string>, vector<string>, char[2], int);
void create_delete_sql_query(string, string, char[2], int);
int string_Hashing(string);
int connect_to_server(server, int);
bool exists_test();
string get_ip_from_actual_server();
string extract_from_data(string);
string extract_where_data(string);
string extract_key_name(string);
string extract_update_data(string);
string extract_insert_into_data_table(string);
string extract_delete_data(string);
string key_extractor(string);
vector<string> extract_insert_into_data_columns(string);
vector<string> extract_values_data(string);
vector<string> extract_select_data(string);
vector<string> extract_set_data(string);
void* Listening_socket(void*);
//ADDED
void send_to_server(int, unsigned char[13], unsigned char[2048]);
void BENCH_redirecting(PrepAndExecReq);
//ENDADDED
#pragma endregion Prototypes

int main(int argc, char* argv[])
{
    if (argc == 4) {    //3 arguments
        if (std::string(argv[1]) == "oui")
            bl_UseReplication = true;
        if (std::string(argv[2]) == "bench")
            bl_UseBench = true;
        if (std::string(argv[3]) == "load")
            bl_Load = true;
    }
    else {
        printf("Incorrect Parameters : ./CoucheLogicielle [oui|non] [bench|non] [load|run]\r\n");
        exit_prog(EXIT_FAILURE);
    }
    int CheckThreadCreation = 0;
    if (bl_UseReplication) {
        logs("main() : Starting Proxy...Replication mode selected");
        CheckThreadCreation += pthread_create(&th_INITSocket_Redirection, NULL, INITSocket_Redirection, NULL);
        if (CheckThreadCreation != 0)
            logs("main() : Thread th_INITSocket_Redirection creation failed", ERROR);
        else
            logs("main() : Thread th_INITSocket_Redirection created");
        int a = 0;
        while (a != 1)
        {
            cout << "tape 1 qd la couche logicielle est lancee sur tous les serveurs" << endl;
            cin >> a;
            cout << endl;
        }
    }
    else
        logs("main() : Starting Proxy...Standalone mode selected");
    ConnexionPGSQL();   //Connexion PGSQL

    if (bl_UseReplication) {
        server_identification();
        replication_relation actual_and_subscriber = { actual_server, subscriber_server };
        //std::cout << "Serveur actual_server: " << actual_server.server_id << " | " << actual_server.server_ip_address << " | " << actual_server.server_name << std::endl;
        //std::cout << "Serveur subscriber_server: " << subscriber_server.server_id << " | " << subscriber_server.server_ip_address << " | " << subscriber_server.server_name << std::endl;
        for (int i = 0; i < _THREDS_EXEC_NUMBER; i++)
            CheckThreadCreation += pthread_create(&th_PrepExec[i], NULL, ConnPGSQLPrepStatements, (void*)&actual_and_subscriber /*NULL*/);
        int b = 0;
        while (b != 1)
        {
            cout << "tape 1 qd la redirection est lancee sur tous les serveurs" << endl;
            cin >> b;
            cout << endl;
        }
    }
    else
    {
        //TEMPORARY (quand je le run sur l'un et que je regarde si ça interagit bien avec l'autre)
        /*actual_server = server_D;
        subscriber_server = server_E;
        replication_relation actual_and_subscriber = {actual_server, subscriber_server};*/
        //ENDTEMPORARY
        for (int i = 0; i < _THREDS_EXEC_NUMBER; i++)
            CheckThreadCreation += pthread_create(&th_PrepExec[i], NULL, ConnPGSQLPrepStatements, NULL /*(void*)&actual_and_subscriber*/);
    }
    //Starting threads for sockets
    CheckThreadCreation += pthread_create(&th_FrameData, NULL, TraitementFrameData, NULL);
    CheckThreadCreation += pthread_create(&th_Requests, NULL, TraitementRequests, NULL);
    CheckThreadCreation += pthread_create(&th_PostgreSQL, NULL, SendPGSQL, NULL);
    if (bl_UseReplication) {
        CheckThreadCreation += pthread_create(&th_Redirecting, NULL, redirecting, NULL);
        CheckThreadCreation += pthread_create(&th_Listening_socket, NULL, Listening_socket, NULL);
    }
    //Check if threads have been created
    if (CheckThreadCreation != 0) {
        logs("main() : Error while creating threads", ERROR);
        exit_prog(EXIT_FAILURE);
    }

    //Réception des frames en continu et mise en buffer
    sockServer = CreateSocket();
    sockDataClient = INITSocket(sockServer/*, argv[2]*/);
    CheckThreadCreation += pthread_create(&th_FrameClient, NULL, TraitementFrameClient, NULL);

    if (CheckThreadCreation != 0) {
        logs("main() : Error while creating threads", ERROR);
        exit_prog(EXIT_FAILURE);
    }
    else
        logs("main() : Threads creation success");
    logs("main() : Starting Done");
    initClock(std::chrono::high_resolution_clock::now());
    timestamp("Starting Done", std::chrono::high_resolution_clock::now());
    char_array frameToSend;
    while (exists_test()) {
        sockServer = 0;
        sockServer = recv(sockDataClient, buffData, sizeof(buffData), 0);
        if (sockServer > 0) {
            // timestamp("Received frame", std::chrono::high_resolution_clock::now());
            memcpy(frameToSend.chararray, buffData, sizeof(buffData));
            while (!mtx_q_frames.try_lock()) {}
            l_bufferFrames.push(frameToSend);
            mtx_q_frames.unlock();
            memset(buffData, 0, sizeof(buffData));
            // timestamp("Frame pushed", std::chrono::high_resolution_clock::now());
        }
    }
    printf("Fermeture du programme...\r\n");
    Ending();
    StopSocketThread();
    bl_loop = false;
    closeThreads();
    logs("Fermeture des ports...");
    //closeSockets();
    logs("Libération mémoire");
    //delete res;
    PQfinish(conn);
    // delete conninfo;
    // delete[] argv;
    // delete (void*) ConnPGSQLPrepStatements;
    // delete (void*) INITSocket_Redirection;
    // delete (void*) TraitementFrameData;
    // delete (void*) TraitementRequests;
    // delete (void*) SendPGSQL;
    // delete (void*) redirecting;
    // delete (void*) TraitementFrameClient;
    printf("Fin du programme...\r\n");
    logs("Fin du programme");
    return EXIT_SUCCESS;
}

#pragma region Requests
void* TraitementFrameData(void* arg) {
    unsigned int sommeSize = 0;
    bool bl_partialRequest = false;
    unsigned char partialRequest[2048];
    unsigned char partialHeader[13];
    int sizeheader = 0;
    unsigned char test[65536];
    unsigned char frameData[65536];
    int autoIncrementRequest = 0;
    PrepAndExecReq s_PrepAndExec_ToSend;
    //MOVED
    unsigned char header[13];
    bool bl_lastRequestFrame = false;
    Requests s_Requests;
    //ENDMOVED
    try {
        memset(&test[0], 0, sizeof(test));
        memset(&header[0], 0, sizeof(header));
        while (bl_loop) {
            if (!l_bufferFrames.empty()) {
                if (mtx_q_frames.try_lock()) {
                    memcpy(frameData, l_bufferFrames.front().chararray, sizeof(frameData));
                    l_bufferFrames.pop();
                    mtx_q_frames.unlock();
                    sommeSize = 0;
                    bl_lastRequestFrame = false;
                    if (bl_partialRequest) {
                        if (partialHeader[4] == _EXECUTE_STATEMENT)
                            sizeheader = 9;
                        else
                            sizeheader = 13;
                        memcpy(test, &partialHeader[0], sizeheader);
                        memcpy(&test[sizeheader], &partialRequest[0], sizeof(partialRequest) - sizeheader);
                        for (unsigned int i = sizeheader; i < sizeof(partialRequest); i++) {
                            if (test[i] == 0x00 && test[i + 1] == 0x00 && test[i + 2] == 0x00 && test[i + 3] == 0x00) {
                                if (frameData[0] == 0x00) {
                                    for (int j = 0; j < 2; j++) {
                                        if (frameData[j + 1] != 0) { i++; }  //If frame is cut between multiple 0x00, need always 3 0x00 to separate exec champs
                                    }
                                }
                                else if (frameData[0] == 0x64 && frameData[102] == 0x00) //All champs starts with 0000 000d to separate them
                                    i += 3;
                                memcpy(&test[i], frameData, sizeof(frameData) - i);
                                i = sizeof(partialRequest);
                                bl_partialRequest = false;
                            }
                        }
                    }
                    else {
                        memcpy(test, frameData, sizeof(frameData));
                    }
                    while (!bl_lastRequestFrame && !bl_partialRequest) {
                        autoIncrementRequest++;
                        memcpy(header, &test[sommeSize], 13);
                        memcpy(s_Requests.opcode, &header[4], 1);
                        memcpy(s_Requests.stream, &header[2], 2);
                        s_Requests.origin = sockDataClient;                              //TODO delete?
                        if (s_Requests.opcode[0] != _EXECUTE_STATEMENT) {
                            s_Requests.size = (unsigned int)header[11] * 256 + (unsigned int)header[12];
                            memcpy(s_Requests.request, &test[13 + sommeSize], s_Requests.size);
                            sommeSize += s_Requests.size + 13;              //Request size + header size(13)
                        }
                        else {
                            s_Requests.size = (unsigned int)header[7] * 256 + (unsigned int)header[8];
                            memcpy(s_Requests.request, &test[9 + sommeSize], s_Requests.size);
                            sommeSize += s_Requests.size + 9;
                        }
                        if (test[sommeSize - 1] == 0x00 && test[sommeSize - 2] == 0x00 && test[sommeSize - 3] == 0x00) {          //Checking for partial request
                            bl_partialRequest = true;
                            autoIncrementRequest--;
                        }
                        if (!bl_partialRequest) {
                            switch (s_Requests.opcode[0])
                            {
                            case _QUERY_STATEMENT:
                                if (test[sommeSize] == 0x00 && test[sommeSize + 1] == 0x01 && test[sommeSize + 2] == 0x00) {    //Checking for USE statements
                                    sommeSize = sommeSize + 3;
                                }
                                if (test[sommeSize] == 0x00)        //Checking last frame
                                    bl_lastRequestFrame = true;
                                else if (test[sommeSize - 2] == 0x04)
                                    sommeSize = sommeSize - 2;
                                if (s_Requests.request[0] == 'U' && s_Requests.request[1] == 'S' && s_Requests.request[2] == 'E')
                                    l_bufferRequestsForActualServer.push_front(s_Requests);
                                else if (bl_UseReplication)
                                    l_bufferRequests.push_front(s_Requests);
                                else
                                    l_bufferRequestsForActualServer.push_front(s_Requests);
                                break;
                            case _EXECUTE_STATEMENT:
                                memcpy(s_PrepAndExec_ToSend.head, header, sizeof(header));
                                memcpy(s_PrepAndExec_ToSend.CQLStatement, s_Requests.request, sizeof(s_Requests.request));
                                //CHANGED
                                s_PrepAndExec_ToSend.origin = sockDataClient;
                                if (bl_UseReplication) {
                                    BENCH_redirecting(s_PrepAndExec_ToSend);
                                    cout << "_EXECUTE_STATEMENT envoye a BENCH_redirecting depuis le client" << endl;
                                }
                                else {
                                    AddToQueue(s_PrepAndExec_ToSend);
                                    cout << "_EXECUTE_STATEMENT AddToQueue depuis le client" << endl;
                                }
                                //ENDCHANGED
                                memset(s_PrepAndExec_ToSend.head, 0x00, sizeof(s_PrepAndExec_ToSend.head));
                                memset(s_PrepAndExec_ToSend.CQLStatement, 0x00, sizeof(s_PrepAndExec_ToSend.CQLStatement));
                                if (test[sommeSize] == 0x00)        //Checking last frame
                                    bl_lastRequestFrame = true;
                                break;
                            case _PREPARE_STATEMENT:
                                if (test[sommeSize] == 0x00)        //Checking last frame
                                    bl_lastRequestFrame = true;
                                memcpy(s_PrepAndExec_ToSend.head, header, sizeof(header));
                                memcpy(s_PrepAndExec_ToSend.CQLStatement, s_Requests.request, sizeof(s_Requests.request));
                                //ADDED
                                s_PrepAndExec_ToSend.origin = sockDataClient;
                                cout << "_PREPARE_STATEMENT AddToQueue depuis le client" << endl;
                                //ENDADDED
                                AddToQueue(s_PrepAndExec_ToSend);
                                break;
                            case _OPTIONS_STATEMENT:
                                logs("DO THIS FUCKING ISALIVE REQUEST FRANZICHE", WARNING);
                                break;
                            default:
                                logs("TraitementFrameData() : Type of request unknown : " + std::string((char*)s_Requests.request), ERROR);
                                break;
                            }
                            // if (fichier) {
                            //     fichier << "Requete N° " << autoIncrementRequest << ", Taille : " << s_Requests.size << " : " << s_Requests.request << endl;
                            // }      
                        }
                        else {
                            logs("Partial request", WARNING);
                            memset(partialRequest, 0, sizeof(partialRequest));
                            memset(partialHeader, 0, sizeof(partialHeader));
                            memcpy(&partialRequest[0], &s_Requests.request[0], sizeof(partialRequest));
                            memcpy(&partialHeader[0], &header[0], sizeof(partialHeader));
                            if (partialRequest[0] == 0x00 && partialRequest[1] == 0x00 && partialRequest[2] == 0x00 && partialRequest[3] == 0x00)
                                bl_partialRequest = false;
                        }//Fin de requête
                        memset(&s_Requests.request[0], 0x00, sizeof(s_Requests.request));
                    }//Fin de frame
                    memset(&test[0], 0, sizeof(test));
                    memset(&header[0], 0, sizeof(header));
                    // timestamp("Frame OK", std::chrono::high_resolution_clock::now());
                }
                else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10)); //TODO maybe need to edit
                }
            }
        }
    }
    catch (std::exception const& e) {
        logs("TraitementRequests() : " + std::string(e.what()), ERROR);
    }
    logs("TraitementRequests() : Stopping thread...");
    pthread_exit(NULL);
}

void* TraitementRequests(void* arg) {
    SQLRequests tempReq;
    char TempReq[2048];
    try {
        while (bl_loop) {
            if (l_bufferRequestsForActualServer.size() > 0) {
                // timestamp("Frame for actual server", std::chrono::high_resolution_clock::now());
                memcpy(TempReq, l_bufferRequestsForActualServer.back().request, 2048);
                tempReq.request = std::string(TempReq);
                memcpy(tempReq.stream, l_bufferRequestsForActualServer.back().stream, 2);
                tempReq.origin = l_bufferRequestsForActualServer.back().origin;
                l_bufferRequestsForActualServer.pop_back();
                // timestamp("Calling CQLToSQL", std::chrono::high_resolution_clock::now());
                CQLtoSQL(tempReq);
            }
        }
    }
    catch (std::exception const& e) {
        logs("TraitementRequests() : " + std::string(e.what()), ERROR);
    }
    pthread_exit(NULL);
}

#pragma endregion Requests

#pragma region PostgreSQL
void ConnexionPGSQL() {
    //timestamp("Starting PGSQL Connexion", std::chrono::high_resolution_clock::now());
    conn = PQconnectdb(conninfo);
    /* Check to see that the backend connection was successfully made */
    if (PQstatus(conn) != CONNECTION_OK)
    {
        logs("ConnexionPGSQL() : Connexion to database failed", ERROR);
        exit_prog(EXIT_FAILURE);
    }
    /* Set always-secure search path, so malicious users can't take control. */
    res = PQexec(conn, "SELECT pg_catalog.set_config('search_path', 'public', false)");
    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        PQclear(res);   //TODO check what is it?
        logs("ConnexionPGSQL() : Secure search path error", ERROR);
        exit_prog(EXIT_FAILURE);
    }
    else
        logs("ConnexionPGSQL() : Connexion to PostgreSQL sucess");
    PQclear(res);
    //timestamp("PGSQL Connexion Done", std::chrono::high_resolution_clock::now());
}

void* SendPGSQL(void* arg)
{
    PGresult* res;
    char requestPGSQL[1024];
    unsigned char stream[2];
    char key_name[255];
    char key[255];
    int pos_key;
    int origin;
    while (bl_loop)
    {
        if (l_bufferPGSQLRequests.size() > 0)
        {
            strcpy(requestPGSQL, l_bufferPGSQLRequests.back().request.c_str());
            memcpy(stream, l_bufferPGSQLRequests.back().stream, 2);
            strcpy(key_name, l_bufferPGSQLRequests.back().key_name);
            strcpy(key, l_bufferPGSQLRequests.back().key);
            pos_key = l_bufferPGSQLRequests.back().pos_key;
            origin = l_bufferPGSQLRequests.back().origin;   //TODO remove?

            l_bufferPGSQLRequests.pop_back();

            cout << requestPGSQL << endl;
            cout << stream << endl;
            cout << key_name << endl;
            cout << key << endl;
            cout << pos_key << endl;

            res = PQexec(conn, requestPGSQL);
            if (PQresultStatus(res) == PGRES_TUPLES_OK)     //(uniquement pour les SELECT, sinon rien)
            {
                //Affichage des En-têtes
                int nFields = PQnfields(res);
                printf("\n\n");
                for (int i = 0; i < nFields; i++)
                    printf("%-15s", PQfname(res, i));
                printf("\n\n");

                unsigned char full_text[15000];
                unsigned char response[38] = {
                    0x84, 0x00, stream[0], stream[1], 0x08, 0x00, 0x00, 'x', 'x', 0x00, 0x00, 0x00, 0x02, 0x00,
                    0x00, 0x00, 0x01, 0x00, 0x00, (nFields + 1) / 256, nFields + 1, 0x00, 0x04, 0x79, 0x63, 0x73, 0x62, 0x00, 0x09, 0x75,
                    0x73, 0x65, 0x72, 0x74, 0x61, 0x62, 0x6c, 0x65 };
                int pos = 0;
                int pos_key_run = pos_key;

                memcpy(full_text + pos, response, 38);
                pos += 38;

                //Toujours deux colonnes normalement      

                //NOMS DE COLONNES    
                for (int i = 0; i < PQntuples(res); i++)
                {
                    if (i == pos_key_run)
                    {
                        printf("%-15s", key_name);

                        unsigned char element_size[2] = { strlen(key_name) / 256, strlen(key_name) };
                        memcpy(full_text + pos, element_size, 2);
                        pos += 2;

                        memcpy(full_text + pos, key_name, strlen(key_name));
                        pos += strlen(key_name);

                        memcpy(full_text + pos, perm_column_separator, 2);
                        pos += 2;

                        i--;
                        pos_key_run = -1;
                    }
                    else
                    {
                        printf("%-15s", PQgetvalue(res, i, 0));

                        unsigned char element_size[2] = { strlen(PQgetvalue(res, i, 0)) / 256, strlen(PQgetvalue(res, i, 0)) };
                        memcpy(full_text + pos, element_size, 2);
                        pos += 2;

                        memcpy(full_text + pos, PQgetvalue(res, i, 0), strlen(PQgetvalue(res, i, 0)));
                        pos += strlen(PQgetvalue(res, i, 0));

                        memcpy(full_text + pos, perm_column_separator, 2);
                        pos += 2;
                    }
                }
                memcpy(full_text + pos, perm_column_value_separation, 4);
                pos += 4;
                pos_key_run = pos_key;
                //VALEURS
                for (int i = 0; i < PQntuples(res); i++)
                {
                    if (i == pos_key_run)
                    {
                        printf("%-15s", key);

                        if (key != NULL)
                        {
                            memcpy(full_text + pos, perm_double_zero, 2);
                            pos += 2;

                            unsigned char element_size[2] = { strlen(key) / 256, strlen(key) };

                            memcpy(full_text + pos, element_size, 2);
                            pos += 2;

                            memcpy(full_text + pos, key, strlen(key));
                            pos += strlen(key);
                        }
                        else
                        {
                            memcpy(full_text + pos, perm_null_element, 4);
                            pos += 4;
                        }

                        i--;
                        pos_key_run = -1;
                    }
                    else
                    {
                        printf("%-15s", PQgetvalue(res, i, 1));

                        if (PQgetvalue(res, i, 1) != NULL)
                        {
                            memcpy(full_text + pos, perm_double_zero, 2);
                            pos += 2;

                            unsigned char element_size[2] = { strlen(PQgetvalue(res, i, 1)) / 256, strlen(PQgetvalue(res, i, 1)) };

                            memcpy(full_text + pos, element_size, 2);
                            pos += 2;

                            memcpy(full_text + pos, PQgetvalue(res, i, 1), strlen(PQgetvalue(res, i, 1)));
                            pos += strlen(PQgetvalue(res, i, 1));
                        }
                        else
                        {
                            memcpy(full_text + pos, perm_null_element, 4);
                            pos += 4;
                        }
                    }
                }

                full_text[7] = (pos - 9) / 256;
                full_text[8] = pos - 9;

                /*if (origin == 0)
                    write(sockDataClient, full_text, pos);
                else*/
                write(origin, full_text, pos);
                cout << "Reponse renvoyee a l'expediteur" << endl;

                printf("\n");
            }

            else if (PQresultStatus(res) == PGRES_COMMAND_OK)
            {
                cout << "Command OK" << endl;
                unsigned char response_cmd_ok[13] = { 0x84, 0x00 , stream[0], stream[1], 0x08, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01 };
                /*if (origin == 0)
                    write(sockDataClient, response_cmd_ok, 13);
                else*/
                write(origin, response_cmd_ok, 13);
                cout << "Reponse renvoyee a l'expediteur" << endl;
            }

            else
            {
                cout << "Erreur dans la requête" << endl;
                cout << PQresultErrorMessage(res);
                //TODO SEND TO CASSANDRA NOK
            }
            PQclear(res);
        }
    }
    logs("SendPGSQL() : Fin du thread");
    pthread_exit(NULL);
    return NULL;
}

#pragma endregion PostgreSQL

#pragma region CQL_SQL
void CQLtoSQL(SQLRequests Request_incoming_cql_query)
{
    fields.clear();
    values.clear();
    columns.clear();
    _incoming_cql_query = Request_incoming_cql_query.request;
    LowerRequest = _incoming_cql_query;
    std::transform(LowerRequest.begin(), LowerRequest.end(), LowerRequest.begin(), [](unsigned char c) { return std::tolower(c); });
    if (LowerRequest.substr(0, 6) == "select")
    {
        try {
            where_sub_pos = limit_sub_pos = sizeof(LowerRequest) - 1;
            cout << "Type de la requete : SELECT" << endl;
            if (LowerRequest.find("from ") == _NPOS)
                throw string("Erreur : Pas de FROM dans la requete");
            else {
                from_sub_pos = LowerRequest.find("from ");
                select_clause = _incoming_cql_query.substr(6, from_sub_pos - 6);
                fields = extract_select_data(select_clause);
            }
            if (LowerRequest.find("where ") == _NPOS) {
                throw string("Erreur : Pas de WHERE dans la requete");  //TODO gérer les cas sans WHERE?
            }
            else {
                where_sub_pos = LowerRequest.find("where ");
                from_clause = _incoming_cql_query.substr(from_sub_pos, where_sub_pos - from_sub_pos);
                table = extract_from_data(from_clause);
            }
            if (LowerRequest.find("limit ") != _NPOS) {
                limit_sub_pos = LowerRequest.find("limit ");
                where_clause = _incoming_cql_query.substr(where_sub_pos, limit_sub_pos - where_sub_pos);
                key = extract_where_data(where_clause);
                key_name = extract_key_name(where_clause);
            }
            //IF WHERE && !LIMIT
            else if (where_sub_pos + 1 < LowerRequest.length()) {
                where_clause = _incoming_cql_query.substr(where_sub_pos, _NPOS);
                key = extract_where_data(where_clause);
                key_name = extract_key_name(where_clause);
            }
            //Affichage des paramètres
            cout << "Table: " << table;
            if (key != "")
                cout << " - Key: " << key << " Fields : ";
            for (string field : fields)
            {
                cout << field << " __ ";
            }
            cout << endl;
            //Appel de la fonction de conversion pour du RWCS
            create_select_sql_query(table, key, fields, Request_incoming_cql_query.stream, key_name, pos_key, Request_incoming_cql_query.origin);
        }
        catch (std::string const& chaine)
        {
            cerr << chaine << endl;
            logs(chaine);
        }
        catch (std::exception const& e)
        {
            cerr << "ERREUR : " << e.what() << endl;
            logs(e.what());
        }
    }
    else if (LowerRequest.substr(0, 6) == "update")
    {
        try {
            cout << "Type de la requete : UPDATE" << endl;
            if (LowerRequest.find("set ") == _NPOS || LowerRequest.find("where ") == _NPOS)   //If missing clause
                throw string("Erreur : SET et|ou WHERE clauses missing");
            //Get clauses positions
            set_sub_pos = LowerRequest.find("set ");
            where_sub_pos = LowerRequest.find("where ");
            //Isolate clauses
            table = _incoming_cql_query.substr(6, set_sub_pos - 6);
            set_clause = _incoming_cql_query.substr(set_sub_pos, where_sub_pos - set_sub_pos);
            where_clause = _incoming_cql_query.substr(where_sub_pos, _NPOS);
            //Extract clauses           
            // table = extract_update_data(update_clause);
            key = extract_where_data(where_clause);
            values = extract_set_data(set_clause);
            //Print clauses
            cout << "Table: " << table << " - Key: " << key << " Fields : ";
            for (unsigned int i = 0; i < values.size(); i = i + 2)
            {
                cout << values[i] << " => " << values[i + 1] << " __ ";
            }
            cout << endl;

            //Call conversion function
            create_update_sql_query(table, key, values, Request_incoming_cql_query.stream, Request_incoming_cql_query.origin);
            // l_bufferPGSQLRequests.push_front(Request_incoming_cql_query);
        }
        catch (std::string const& chaine)
        {
            cerr << chaine << endl;
            logs(chaine);
        }
        catch (std::exception const& e)
        {
            cerr << "ERREUR : " << e.what() << endl;
            logs(e.what());
        }
    }
    else if (LowerRequest.substr(0, 11) == "insert into")
    {
        try {
            cout << "Type de la requete : INSERT" << endl;
            if (LowerRequest.find("values ") == _NPOS)   //If missing clause
                throw string("Erreur : Missing Clause VALUES in INSERT INTO clause");
            //Get clauses positions
            values_sub_pos = LowerRequest.find("values ");
            //Isolate clauses
            insert_into_clause = _incoming_cql_query.substr(11, values_sub_pos - 11);
            values_clause = _incoming_cql_query.substr(values_sub_pos, _NPOS);
            //On extrait les paramètres des clauses
            table = extract_insert_into_data_table(insert_into_clause);
            key = extract_values_data(values_clause)[0];
            columns = extract_insert_into_data_columns(insert_into_clause);
            values = extract_values_data(values_clause);
            //Affichage des paramètres
            cout << "Table: " << table << " - Key: " << key << " Fields : ";
            for (unsigned int i = 0; i < columns.size(); i++)
            {
                cout << columns[i] << " => " << values[i] << " __ ";
            }
            cout << endl;
            create_insert_sql_query(table, key, columns, values, Request_incoming_cql_query.stream, Request_incoming_cql_query.origin);             //Appel de la fonction de conversion pour du RWCS
        }
        catch (std::string const& chaine)
        {
            cerr << chaine << endl;
            logs(chaine);
        }
        catch (std::exception const& e)
        {
            cerr << "ERREUR : " << e.what() << endl;
            logs(e.what());
        }
    }
    else if (LowerRequest.substr(0, 6) == "delete")
    {
        try {
            cout << "Type de la requete : DELETE" << endl;
            if (LowerRequest.find("where ") == _NPOS)   //If missing clause
                throw string("Erreur : Missing Clause WHERE in DELETE clause");
            where_sub_pos = LowerRequest.find("where ");
            //On génère les clauses de la requête
            table = _incoming_cql_query.substr(12, where_sub_pos - 12);
            where_clause = _incoming_cql_query.substr(where_sub_pos, _NPOS);
            //On extrait les paramètres des clauses
            //table = extract_delete_data(delete_clause);
            key = extract_where_data(where_clause);
            //Affichage des paramètres
            cout << "Table: " << table << " - Key: " << key << endl;
            //Appel de la fonction de conversion pour du RWCS
            create_delete_sql_query(table, key, Request_incoming_cql_query.stream, Request_incoming_cql_query.origin);
        }
        catch (std::string const& chaine)
        {
            cerr << chaine << endl;
            logs(chaine);
        }
        catch (std::exception const& e)
        {
            cerr << "ERREUR : " << e.what() << endl;
            logs(e.what());
        }
    }
    else if (LowerRequest.substr(0, 3) == "use") {
        memcpy(&UseResponse[2], &Request_incoming_cql_query.stream, 2);

        /*if (Request_incoming_cql_query.origin == 0)
            write(sockDataClient, UseResponse, sizeof(UseResponse));
        else*/
        write(Request_incoming_cql_query.origin, UseResponse, sizeof(UseResponse));
        cout << "Reponse renvoyee a l'expediteur" << endl;
    }
    else {
        logs("CQLtoSQL() : Type de requete non reconnu. Requete : " + _incoming_cql_query, ERROR);      //TODO peut-être LOG ça dans un fichier de logs de requetes?
    }
    // timestamp("CQLtoSQLDone", std::chrono::high_resolution_clock::now());
}

void create_select_sql_query(string _table, string _key, vector<string> _fields, char id[2], string _key_name, int pos_key, int origin)
{
    SQLRequests TempSQLReq;
    memcpy(TempSQLReq.stream, id, 2);
    strcpy(TempSQLReq.key, _key.c_str());
    strcpy(TempSQLReq.key_name, _key_name.c_str());
    TempSQLReq.pos_key = pos_key;
    TempSQLReq.origin = origin;
    TempSQLReq.request = "SELECT * FROM " + _key;
    //On regarde si, dans la requête CQL, on voulait prendre * ou des champs particuliers
    if (_fields[0] != "*")
    {
        TempSQLReq.request += " WHERE column_name IN ('";
        for (string field : _fields)
        {
            TempSQLReq.request += field + "', '";
        }
        TempSQLReq.request = TempSQLReq.request.substr(0, TempSQLReq.request.length() - 3);
        TempSQLReq.request += ");";
    }
    else {
        TempSQLReq.request += ";";
    }
    l_bufferPGSQLRequests.push_front(TempSQLReq);
    return;
}

void create_update_sql_query(string _table, string _key, vector<string> _values, char id[2], int origin)
{
    //Comme on update uniquement la colonne value d'une table, il faut utiliser la clause CASE sur la valeur de la colonne column_name
    //Dans _values, les éléments "impairs" (le premier, troisième,...) sont les colonnes à update et les "pairs" sont les valeurs de ces colonnes
    SQLRequests TempSQLReq;
    memcpy(TempSQLReq.stream, id, 2);
    TempSQLReq.origin = origin;
    TempSQLReq.request = "UPDATE " + _key + " SET value = ";
    //On regarde si on a plusieurs champs à update
    if (_values.size() > 2)
    {
        TempSQLReq.request += "(CASE ";
        for (unsigned int i = 0; i < _values.size(); i = i + 2)
        {
            //On CASE sur chaque colonne CQL, càd chaque valeur que peut prendre la colonne column_name
            TempSQLReq.request += "when column_name = '" + _values[i] + "' then '" + _values[i + 1] + "' ";
        }
        TempSQLReq.request += "END) WHERE column_name IN ('";
        for (unsigned int i = 0; i < _values.size(); i = i + 2)
        {
            TempSQLReq.request += _values[i] + "', '";
        }
        TempSQLReq.request = TempSQLReq.request.substr(0, TempSQLReq.request.length() - 4);
        TempSQLReq.request += "');";
    }
    else
    {
        TempSQLReq.request += "'" + _values[0] + "' WHERE column_name = '" + _values[1] + "';";
    }
    l_bufferPGSQLRequests.push_front(TempSQLReq);
    return;
}

void create_insert_sql_query(string _table, string _key, vector<string> _columns, vector<string> _values, char id[2], int origin)
{
    SQLRequests TempSQLReq;
    memcpy(TempSQLReq.stream, id, 2);
    TempSQLReq.origin = origin;
    TempSQLReq.request = "CREATE TABLE " + _key + " (column_name varchar(255), value varchar(255)); ";
    // cout<<TempSQLReq.request<<endl;
    l_bufferPGSQLRequests.push_front(TempSQLReq);
    for (unsigned int i = 1; i < _values.size(); i++)
    {
        TempSQLReq.request = "INSERT INTO " + _key + " (column_name, value) VALUES('" + _columns[i] + "', '" + _values[i] + "'); ";
        // cout<<TempSQLReq.request<<endl;
        l_bufferPGSQLRequests.push_front(TempSQLReq);
    }
    //TODO
    //return returned_insert_sql_query;
}

void create_delete_sql_query(string _table, string _key, char id[2], int origin)
{
    SQLRequests TempSQLReq;
    memcpy(TempSQLReq.stream, id, 2);
    TempSQLReq.origin = origin;
    TempSQLReq.request = "DROP TABLE " + _key + " ;";
    l_bufferPGSQLRequests.push_front(TempSQLReq);
    return;
}

vector<string> extract_select_data(string _select_clause)
{
    vector<string> returned_vector;
    size_t pos = 0;
    string token;

    while ((pos = _select_clause.find(',')) != _NPOS)
    {

        token = _select_clause.substr(0, pos);
        if (token.find('.') != _NPOS)
        {
            token = token.substr(token.find('.') + 1);  //Why?
        }
        // token.erase(remove(token.begin(), token.end(), '('), token.end());   //Pas utile selon moi
        // token.erase(remove(token.begin(), token.end(), '\''), token.end());
        token.erase(remove(token.begin(), token.end(), ' '), token.end());
        // token.erase(remove(token.begin(), token.end(), ';'), token.end());
        // token.erase(remove(token.begin(), token.end(), ')'), token.end());

        returned_vector.push_back(token);
        _select_clause.erase(0, pos + 1);
    }

    if (_select_clause.find('.') != _NPOS)
    {
        _select_clause = _select_clause.substr(_select_clause.find('.') + 1);   //Again? Si vraiment utile, à mettre avec celui du dessus?
    }
    // _select_clause.erase(remove(_select_clause.begin(), _select_clause.end(), '('), _select_clause.end());
    // _select_clause.erase(remove(_select_clause.begin(), _select_clause.end(), '\''), _select_clause.end());
    _select_clause.erase(remove(_select_clause.begin(), _select_clause.end(), ' '), _select_clause.end());
    // _select_clause.erase(remove(_select_clause.begin(), _select_clause.end(), ';'), _select_clause.end());
    // _select_clause.erase(remove(_select_clause.begin(), _select_clause.end(), ')'), _select_clause.end());

    returned_vector.push_back(_select_clause);

    return returned_vector;
}

string extract_from_data(string _form_clause)
{
    string form_clause_data = _form_clause.substr(5, _NPOS);

    form_clause_data.erase(remove(form_clause_data.begin(), form_clause_data.end(), '('), form_clause_data.end());  //OK
    form_clause_data.erase(remove(form_clause_data.begin(), form_clause_data.end(), '\''), form_clause_data.end());
    form_clause_data.erase(remove(form_clause_data.begin(), form_clause_data.end(), ';'), form_clause_data.end());
    form_clause_data.erase(remove(form_clause_data.begin(), form_clause_data.end(), ' '), form_clause_data.end());
    form_clause_data.erase(remove(form_clause_data.begin(), form_clause_data.end(), ')'), form_clause_data.end());

    return form_clause_data;
}

string extract_where_data(string _where_clause_data)
{
    string where_clause_data = _where_clause_data;
    where_clause_data = where_clause_data.substr(6, _NPOS);

    size_t pos = 0;

    where_clause_data.erase(remove(where_clause_data.begin(), where_clause_data.end(), '('), where_clause_data.end());
    where_clause_data.erase(remove(where_clause_data.begin(), where_clause_data.end(), '\''), where_clause_data.end());
    where_clause_data.erase(remove(where_clause_data.begin(), where_clause_data.end(), ' '), where_clause_data.end());
    where_clause_data.erase(remove(where_clause_data.begin(), where_clause_data.end(), ';'), where_clause_data.end());
    where_clause_data.erase(remove(where_clause_data.begin(), where_clause_data.end(), ')'), where_clause_data.end());

    while ((pos = where_clause_data.find('=')) != _NPOS)
    {
        where_clause_data.erase(0, pos + 1);
    }

    return where_clause_data;
}

string extract_key_name(string _where_clause_data)
{
    string where_clause_data = _where_clause_data;
    where_clause_data = where_clause_data.substr(6, _NPOS);

    size_t pos = 0;

    where_clause_data.erase(remove(where_clause_data.begin(), where_clause_data.end(), '('), where_clause_data.end());
    where_clause_data.erase(remove(where_clause_data.begin(), where_clause_data.end(), '\''), where_clause_data.end());
    where_clause_data.erase(remove(where_clause_data.begin(), where_clause_data.end(), ' '), where_clause_data.end());
    where_clause_data.erase(remove(where_clause_data.begin(), where_clause_data.end(), ';'), where_clause_data.end());
    where_clause_data.erase(remove(where_clause_data.begin(), where_clause_data.end(), ')'), where_clause_data.end());

    pos = where_clause_data.find('=');
    where_clause_data = where_clause_data.substr(0, pos);

    return where_clause_data;
}

string extract_update_data(string _update_clause)
{
    string update_clause_data = _update_clause.substr(7, _NPOS);

    update_clause_data.erase(remove(update_clause_data.begin(), update_clause_data.end(), '('), update_clause_data.end());
    update_clause_data.erase(remove(update_clause_data.begin(), update_clause_data.end(), '\''), update_clause_data.end());
    update_clause_data.erase(remove(update_clause_data.begin(), update_clause_data.end(), ' '), update_clause_data.end());
    update_clause_data.erase(remove(update_clause_data.begin(), update_clause_data.end(), ';'), update_clause_data.end());
    update_clause_data.erase(remove(update_clause_data.begin(), update_clause_data.end(), ')'), update_clause_data.end());

    return update_clause_data;
}

vector<string> extract_set_data(string set_clause_data)
{
    set_clause_data = set_clause_data.substr(3, _NPOS);
    vector<string> returned_vector;

    size_t pos = 0;
    size_t pos_bis = 0;
    string token;
    string token_bis;

    while ((pos = set_clause_data.find(',')) != _NPOS)
    {
        token = set_clause_data.substr(0, pos);

        while ((pos_bis = token.find('=')) != _NPOS)
        {
            token_bis = token.substr(0, pos_bis);
            if (token_bis.find('.') != _NPOS)
            {
                token_bis = token_bis.substr(token.find('.') + 1);
            }
            token_bis.erase(remove(token_bis.begin(), token_bis.end(), '('), token_bis.end());
            token_bis.erase(remove(token_bis.begin(), token_bis.end(), '\''), token_bis.end());
            token_bis.erase(remove(token_bis.begin(), token_bis.end(), ' '), token_bis.end());
            token_bis.erase(remove(token_bis.begin(), token_bis.end(), ';'), token_bis.end());
            token_bis.erase(remove(token_bis.begin(), token_bis.end(), ')'), token_bis.end());

            returned_vector.push_back(token_bis);
            token.erase(0, pos_bis + 1);

            token.erase(remove(token.begin(), token.end(), '('), token.end());
            token.erase(remove(token.begin(), token.end(), '\''), token.end());
            token.erase(remove(token.begin(), token.end(), ' '), token.end());
            token.erase(remove(token.begin(), token.end(), ';'), token.end());
            token.erase(remove(token.begin(), token.end(), ')'), token.end());

            returned_vector.push_back(token);
        }
        set_clause_data.erase(0, pos + 1);
    }

    while ((pos = set_clause_data.find('=')) != _NPOS)
    {
        token = set_clause_data.substr(0, pos);
        if (token.find('.') != _NPOS)
        {
            token = token.substr(token.find('.') + 1);
        }
        token.erase(remove(token.begin(), token.end(), '('), token.end());
        token.erase(remove(token.begin(), token.end(), '\''), token.end());
        token.erase(remove(token.begin(), token.end(), ' '), token.end());
        token.erase(remove(token.begin(), token.end(), ';'), token.end());
        token.erase(remove(token.begin(), token.end(), ')'), token.end());

        returned_vector.push_back(token);
        set_clause_data.erase(0, pos + 1);

        set_clause_data.erase(remove(set_clause_data.begin(), set_clause_data.end(), '('), set_clause_data.end());
        set_clause_data.erase(remove(set_clause_data.begin(), set_clause_data.end(), '\''), set_clause_data.end());
        set_clause_data.erase(remove(set_clause_data.begin(), set_clause_data.end(), ' '), set_clause_data.end());
        set_clause_data.erase(remove(set_clause_data.begin(), set_clause_data.end(), ';'), set_clause_data.end());
        set_clause_data.erase(remove(set_clause_data.begin(), set_clause_data.end(), ')'), set_clause_data.end());

        returned_vector.push_back(set_clause_data);
    }

    return returned_vector;
}

string extract_insert_into_data_table(string insert_into_clause_data)
{
    insert_into_clause_data = insert_into_clause_data.substr(0, insert_into_clause_data.find("("));

    return insert_into_clause_data;
}

vector<string> extract_insert_into_data_columns(string insert_into_clause_data)
{
    vector<string> returned_vector;

    size_t pos = 0;
    string token;

    insert_into_clause_data.erase(0, insert_into_clause_data.find("("));

    while ((pos = insert_into_clause_data.find(',')) != _NPOS)
    {
        token = insert_into_clause_data.substr(0, pos);
        if (token.find('.') != _NPOS)
        {
            token = token.substr(token.find('.') + 1);
        }
        token.erase(remove(token.begin(), token.end(), '('), token.end());
        token.erase(remove(token.begin(), token.end(), '\''), token.end());
        token.erase(remove(token.begin(), token.end(), ' '), token.end());
        token.erase(remove(token.begin(), token.end(), ';'), token.end());
        token.erase(remove(token.begin(), token.end(), ')'), token.end());

        returned_vector.push_back(token);
        insert_into_clause_data.erase(0, pos + 1);
    }

    if (insert_into_clause_data.find('.') != _NPOS)
    {
        insert_into_clause_data = insert_into_clause_data.substr(insert_into_clause_data.find('.') + 1);
    }
    insert_into_clause_data.erase(remove(insert_into_clause_data.begin(), insert_into_clause_data.end(), '('), insert_into_clause_data.end());
    insert_into_clause_data.erase(remove(insert_into_clause_data.begin(), insert_into_clause_data.end(), '\''), insert_into_clause_data.end());
    insert_into_clause_data.erase(remove(insert_into_clause_data.begin(), insert_into_clause_data.end(), ' '), insert_into_clause_data.end());
    insert_into_clause_data.erase(remove(insert_into_clause_data.begin(), insert_into_clause_data.end(), ';'), insert_into_clause_data.end());
    insert_into_clause_data.erase(remove(insert_into_clause_data.begin(), insert_into_clause_data.end(), ')'), insert_into_clause_data.end());

    returned_vector.push_back(insert_into_clause_data);

    return returned_vector;
}

vector<string> extract_values_data(string values_clause_data)
{
    values_clause_data = values_clause_data.substr(6, _NPOS);
    vector<string> returned_vector;

    size_t pos = 0;
    string token;

    while ((pos = values_clause_data.find(',')) != _NPOS)
    {
        token = values_clause_data.substr(0, pos);
        if (token.find('.') != _NPOS)
        {
            token = token.substr(token.find('.') + 1);
        }
        token.erase(remove(token.begin(), token.end(), '('), token.end());
        token.erase(remove(token.begin(), token.end(), '\''), token.end());
        token.erase(remove(token.begin(), token.end(), ' '), token.end());
        token.erase(remove(token.begin(), token.end(), ';'), token.end());
        token.erase(remove(token.begin(), token.end(), ')'), token.end());

        returned_vector.push_back(token);
        values_clause_data.erase(0, pos + 1);
    }

    if (values_clause_data.find('.') != _NPOS)
    {
        values_clause_data = values_clause_data.substr(values_clause_data.find('.') + 1);
    }
    values_clause_data.erase(remove(values_clause_data.begin(), values_clause_data.end(), '('), values_clause_data.end());
    values_clause_data.erase(remove(values_clause_data.begin(), values_clause_data.end(), '\''), values_clause_data.end());
    values_clause_data.erase(remove(values_clause_data.begin(), values_clause_data.end(), ' '), values_clause_data.end());
    values_clause_data.erase(remove(values_clause_data.begin(), values_clause_data.end(), ';'), values_clause_data.end());
    values_clause_data.erase(remove(values_clause_data.begin(), values_clause_data.end(), ')'), values_clause_data.end());

    returned_vector.push_back(values_clause_data);

    return returned_vector;
}

string extract_delete_data(string _delete_clause)
{
    string delete_clause_data = _delete_clause.substr(12, _NPOS);

    delete_clause_data.erase(remove(delete_clause_data.begin(), delete_clause_data.end(), '('), delete_clause_data.end());
    delete_clause_data.erase(remove(delete_clause_data.begin(), delete_clause_data.end(), '\''), delete_clause_data.end());
    delete_clause_data.erase(remove(delete_clause_data.begin(), delete_clause_data.end(), ' '), delete_clause_data.end());
    delete_clause_data.erase(remove(delete_clause_data.begin(), delete_clause_data.end(), ';'), delete_clause_data.end());
    delete_clause_data.erase(remove(delete_clause_data.begin(), delete_clause_data.end(), ')'), delete_clause_data.end());

    return delete_clause_data;
}
#pragma endregion CQL_SQL

#pragma region Utils

void exit_prog(int codeEXIT) {

    logs("exit_prog() : Fin du programme...");
    exit(codeEXIT);
}

inline bool exists_test() {
    return (stat(fileExistRun.c_str(), &buffe) == 0);
}

void closeSockets() {
    int closeSocketResult = 0;
    closeSocketResult += close(sockDataClient);
    closeSocketResult += close(GetSocketConn());
    closeSocketResult += close(client_connection);
    printf("Sock close : %d\r\n", closeSocketResult);
    if (closeSocketResult == 0)
        logs("Sockets closed");
    else {
        logs("Socket closing", ERROR);
    }
    return;
}

void closeThreads() {
    // closethreads+= pthread_cancel(th_PrepExec1);
    // closethreads+=pthread_cancel(th_PrepExec2);
    // closethreads+=pthread_cancel(th_PrepExec3);
    // closethreads+=pthread_cancel(th_PrepExec4);
    // closethreads+=pthread_cancel(th_PrepExec5);
    // closethreads+=pthread_cancel(th_PrepExec6);
    // closethreads+=pthread_cancel(th_PrepExec7);
    // closethreads+=pthread_cancel(th_PrepExec8);
    for (int i = 0; i < _THREDS_EXEC_NUMBER; i++) {
        pthread_join(th_PrepExec[i], NULL);
    }
    // pthread_join(th_PrepExec1, NULL);
    // pthread_join(th_PrepExec2, NULL);
    // pthread_join(th_PrepExec3, NULL);
    // pthread_join(th_PrepExec4, NULL);
    // pthread_join(th_PrepExec5, NULL);
    // pthread_join(th_PrepExec6, NULL);
    // pthread_join(th_PrepExec7, NULL);
    // pthread_join(th_PrepExec8, NULL);
    printf("1\r\n");
    pthread_join(th_Requests, NULL);
    printf("1\r\n");
    pthread_join(th_PostgreSQL, NULL);
    printf("1\r\n");
    pthread_join(th_FrameClient, NULL);
    printf("1\r\n");
    pthread_join(th_FrameData, NULL);
    printf("1\r\n");
    pthread_join(th_Redirecting, NULL);
    printf("1\r\n");
    pthread_join(th_INITSocket_Redirection, NULL);
}
#pragma endregion Utils

#pragma region Listening
void* INITSocket_Redirection(void* arg)
{
    struct sockaddr_in address;
    unsigned char buffer[65536];

    socket_for_client = socket(AF_INET, SOCK_STREAM, 0);
    memset(&address, '0', sizeof(address));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    bind(socket_for_client, (struct sockaddr*)&address, sizeof(address));

    listen(socket_for_client, 10);

    while (accepted_connections.size() < server_count - 1)
    {
        client_connection = accept(socket_for_client, (struct sockaddr*)NULL, NULL);
        fcntl(client_connection, F_SETFL, O_NONBLOCK);
        accepted_connections.push_back(client_connection);
        client_connection = 0;
        cout << "Connexion " << accepted_connections.size() << " acceptee pour la redirection" << endl;
    }

    cout << "Connexion a tous les serveurs acceptees!" << endl;

    while (1)
    {
        for (int i = 0; i < accepted_connections.size(); i++)
        {
            if (recv(accepted_connections[i], buffer, sizeof(buffer), 0) > 0)
            {
                //ADDED
                unsigned int sommeSize = 0;
                bool bl_partialRequest = false;
                unsigned char partialRequest[2048];
                unsigned char partialHeader[13];
                int sizeheader = 0;
                unsigned char test[65536];
                unsigned char frameData[65536];
                int autoIncrementRequest = 0;
                PrepAndExecReq s_PrepAndExec_ToSend;
                unsigned char header[13];
                bool bl_lastRequestFrame = false;
                Requests s_Requests;
                try {
                    cout << "_PrepAndExecReq reçue dans INITSocket_Redirection" << endl;
                    memset(&test[0], 0, sizeof(test));
                    memset(&header[0], 0, sizeof(header));
                    memcpy(frameData, buffer, sizeof(buffer));
                    sommeSize = 0;
                    bl_lastRequestFrame = false;
                    if (bl_partialRequest) {
                        if (partialHeader[4] == _EXECUTE_STATEMENT)
                            sizeheader = 9;
                        else
                            sizeheader = 13;
                        memcpy(test, &partialHeader[0], sizeheader);
                        memcpy(&test[sizeheader], &partialRequest[0], sizeof(partialRequest) - sizeheader);
                        for (unsigned int i = sizeheader; i < sizeof(partialRequest); i++) {
                            if (test[i] == 0x00 && test[i + 1] == 0x00 && test[i + 2] == 0x00 && test[i + 3] == 0x00) {
                                if (frameData[0] == 0x00) {
                                    for (int j = 0; j < 2; j++) {
                                        if (frameData[j + 1] != 0) { i++; }  //If frame is cut between multiple 0x00, need always 3 0x00 to separate exec champs
                                    }
                                }
                                else if (frameData[0] == 0x64 && frameData[102] == 0x00) //All champs starts with 0000 000d to separate them
                                    i += 3;
                                memcpy(&test[i], frameData, sizeof(frameData) - i);
                                i = sizeof(partialRequest);
                                bl_partialRequest = false;
                            }
                        }
                    }
                    else {
                        memcpy(test, frameData, sizeof(frameData));
                    }
                    while (!bl_lastRequestFrame && !bl_partialRequest) {
                        autoIncrementRequest++;
                        memcpy(header, &test[sommeSize], 13);
                        memcpy(s_Requests.opcode, &header[4], 1);
                        memcpy(s_Requests.stream, &header[2], 2);
                        s_Requests.origin = accepted_connections[i];                              //TODO delete?
                        if (s_Requests.opcode[0] != _EXECUTE_STATEMENT) {
                            s_Requests.size = (unsigned int)header[11] * 256 + (unsigned int)header[12];
                            memcpy(s_Requests.request, &test[13 + sommeSize], s_Requests.size);
                            sommeSize += s_Requests.size + 13;              //Request size + header size(13)
                        }
                        else {
                            s_Requests.size = (unsigned int)header[7] * 256 + (unsigned int)header[8];
                            memcpy(s_Requests.request, &test[13 + sommeSize], s_Requests.size);
                            sommeSize += s_Requests.size + 13;
                        }
                        if (test[sommeSize - 1] == 0x00 && test[sommeSize - 2] == 0x00 && test[sommeSize - 3] == 0x00) {          //Checking for partial request
                            bl_partialRequest = true;
                            autoIncrementRequest--;
                        }
                        if (!bl_partialRequest) {
                            switch (s_Requests.opcode[0])
                            {
                            case _QUERY_STATEMENT:
                                if (test[sommeSize] == 0x00 && test[sommeSize + 1] == 0x01 && test[sommeSize + 2] == 0x00) {    //Checking for USE statements
                                    sommeSize = sommeSize + 3;
                                }
                                if (test[sommeSize] == 0x00)        //Checking last frame
                                    bl_lastRequestFrame = true;
                                else if (test[sommeSize - 2] == 0x04)
                                    sommeSize = sommeSize - 2;
                                if (s_Requests.request[0] == 'U' && s_Requests.request[1] == 'S' && s_Requests.request[2] == 'E')
                                    l_bufferRequestsForActualServer.push_front(s_Requests);
                                /*else if (bl_UseReplication)
                                    l_bufferRequests.push_front(s_Requests);*/
                                else
                                    l_bufferRequestsForActualServer.push_front(s_Requests);
                                break;
                            case _EXECUTE_STATEMENT:
                                memcpy(s_PrepAndExec_ToSend.head, header, sizeof(header));
                                memcpy(s_PrepAndExec_ToSend.CQLStatement, s_Requests.request, sizeof(s_Requests.request));
                                //ADDED
                                s_PrepAndExec_ToSend.origin = accepted_connections[i];
                                cout << "_EXECUTE_STATEMENT AddToQueue depuis INITSocket_Redirection" << endl;
                                //ENDADDED
                                AddToQueue(s_PrepAndExec_ToSend);
                                memset(s_PrepAndExec_ToSend.head, 0x00, sizeof(s_PrepAndExec_ToSend.head));
                                memset(s_PrepAndExec_ToSend.CQLStatement, 0x00, sizeof(s_PrepAndExec_ToSend.CQLStatement));
                                if (test[sommeSize] == 0x00)        //Checking last frame
                                    bl_lastRequestFrame = true;
                                break;
                            case _PREPARE_STATEMENT:
                                if (test[sommeSize] == 0x00)        //Checking last frame
                                    bl_lastRequestFrame = true;
                                memcpy(s_PrepAndExec_ToSend.head, header, sizeof(header));
                                memcpy(s_PrepAndExec_ToSend.CQLStatement, s_Requests.request, sizeof(s_Requests.request));
                                //ADDED
                                s_PrepAndExec_ToSend.origin = accepted_connections[i];
                                /*cout << "REDIRECTION_SIZE_PREPARE_STATEMENT: " << s_Requests.size << endl;
                                cout << "_PREPARE_STATEMENT AddToQueue depuis INITSocket_Redirection" << endl;
                                cout << "_PrepAndExecReq.head: " << endl;
                                for(int i = 0; i < 13; i++)
                                    cout << s_PrepAndExec_ToSend.head[i];
                                cout << " _PrepAndExecReq.CQLStatement: " << endl;
                                for(int i = 0; i < s_Requests.size; i++)
                                    cout <<  s_PrepAndExec_ToSend.CQLStatement[i];*/
                                    //ENDADDED
                                AddToQueue(s_PrepAndExec_ToSend);
                                break;
                            case _OPTIONS_STATEMENT:
                                logs("DO THIS FUCKING ISALIVE REQUEST FRANZICHE", WARNING);
                                break;
                            default:
                                logs("TraitementFrameData() : Type of request unknown : " + std::string((char*)s_Requests.request), ERROR);
                                break;
                            }
                            // if (fichier) {
                            //     fichier << "Requete N° " << autoIncrementRequest << ", Taille : " << s_Requests.size << " : " << s_Requests.request << endl;
                            // }      
                        }
                        else {
                            logs("Partial request", WARNING);
                            memset(partialRequest, 0, sizeof(partialRequest));
                            memset(partialHeader, 0, sizeof(partialHeader));
                            memcpy(&partialRequest[0], &s_Requests.request[0], sizeof(partialRequest));
                            memcpy(&partialHeader[0], &header[0], sizeof(partialHeader));
                            if (partialRequest[0] == 0x00 && partialRequest[1] == 0x00 && partialRequest[2] == 0x00 && partialRequest[3] == 0x00)
                                bl_partialRequest = false;
                        }//Fin de requête
                        memset(&s_Requests.request[0], 0x00, sizeof(s_Requests.request));
                    }//Fin de frame
                    memset(&test[0], 0, sizeof(test));
                    memset(&header[0], 0, sizeof(header));
                    // timestamp("Frame OK", std::chrono::high_resolution_clock::now());
                }
                catch (std::exception const& e) {
                    logs("TraitementRequests() : " + std::string(e.what()), ERROR);
                }
                //ENDADDED
            }
        }
    }

    return NULL;
}
#pragma endregion Listening

#pragma region Preparation
void server_identification()
{
    for (int i = 0; i < l_servers.size(); i++)
    {
        if (get_ip_from_actual_server() == l_servers[i].server_ip_address)
        {
            actual_server = l_servers[i];
            subscriber_server = l_servers[(i + 1) % l_servers.size()];
            cout << get_ip_from_actual_server() << endl;
            cout << "Serveur #" << actual_server.server_id << ", Nom : " << actual_server.server_name << ", Adresse IP: " << actual_server.server_ip_address << endl;
        }
        else
        {
            connected_connections.push_back(connect_to_server(l_servers[i], port));
            cout << "Tentative de connexion avec " << l_servers[i].server_name << " (" << l_servers[i].server_ip_address << ") " << endl;
        }
    }
}

string get_ip_from_actual_server() {
    struct ifaddrs* ifAddrStruct = NULL;
    struct ifaddrs* ifa = NULL;
    void* tmpAddrPtr = NULL;

    string address;

    getifaddrs(&ifAddrStruct);

    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr)
            continue;

        if (ifa->ifa_addr->sa_family == AF_INET) // check it is IP4
        {
            tmpAddrPtr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            if (strcmp(ifa->ifa_name, "eth0") == 0)
                address = addressBuffer;
        }
    }
    if (ifAddrStruct != NULL)
        freeifaddrs(ifAddrStruct);

    return address;
}

#pragma endregion Preparation

#pragma region Server_connection
int connect_to_server(server _server_to_connect, int _port_to_connect)
{
    int sock_to_server = 0;     //TODO delete multiple declaration, not in priority
    struct sockaddr_in serv_addr;
    const char* ip_address = _server_to_connect.server_ip_address.c_str();  //MIHALY remove this line?

    sock_to_server = socket(AF_INET, SOCK_STREAM, 0);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(_port_to_connect);

    inet_pton(AF_INET, ip_address, &serv_addr.sin_addr);    //MIHALY remove this line?
    connect(sock_to_server, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    fcntl(sock_to_server, F_SETFL, O_NONBLOCK);
    logs("connect_to_server() : Connexion établie avec " + _server_to_connect.server_ip_address);
    // cout << endl << "Connexion etablie avec " << ip_address << endl;

    return sock_to_server;
}

void send_to_server(int _socketServer, char _stream_to_send[2], string _query_to_send)
{
    unsigned char cql_query[13 + _query_to_send.length()];
    unsigned char header_to_send[13] = { 0x04, 0x00, _stream_to_send[0], _stream_to_send[1], 0x07, 0x00, 0x00, (_query_to_send.length() + 21) / 256, _query_to_send.length() + 21, 0x00, 0x00, _query_to_send.length() / 256, _query_to_send.length() };

    memcpy(cql_query, header_to_send, 13);
    memcpy(cql_query + 13, _query_to_send.c_str(), _query_to_send.length());

    write(_socketServer, cql_query, 13 + _query_to_send.length());
    cout << endl << "Incoming query sent" << endl;
}

//ADDED
void send_to_server(int _socketServer, unsigned char _head[13], unsigned char _CQLStatement[2048])
{
    int size = (unsigned int)_head[7] * 256 + (unsigned int)_head[8];
    /*cout << "SIZE 7/8 at send: " << size << endl;*/
    unsigned char cql_query[13 + size];
    memset(cql_query, 0, sizeof(cql_query));

    memcpy(cql_query, _head, sizeof(_head));
    memcpy(cql_query + 13, _CQLStatement, size);

    write(_socketServer, cql_query, sizeof(cql_query));

    /*cout << "_PrepAndExecReq.head: " << endl;
    for(int i = 0; i < sizeof(_head); i++)
        cout << cql_query[i];
    cout << " _PrepAndExecReq.CQLStatement: " << endl;
    for(int i = 0; i < size; i++)
        cout <<  cql_query[13 + i];*/
    cout << endl << "PrepAndExecReq write depuis BENCH_redirecting" << endl;
}
//ENDADDED
#pragma endregion Server_connection

#pragma region Redirecting
void* redirecting(void* arg)
{
    Requests req;
    string tempReq;
    while (1)
    {
        if (l_bufferRequests.size() > 0)
        {
            tempReq = (char*)l_bufferRequests.back().request;
            req.origin = l_bufferRequests.back().origin;
            memcpy(req.stream, l_bufferRequests.back().stream, 2);
            memcpy(req.opcode, l_bufferRequests.back().opcode, 1);
            l_bufferRequests.pop_back();
            cout << "redirecting() pop back ok" << endl;

            string key_from_cql_query = key_extractor(tempReq);

            //On hash la clé extraite de la requête via la fonction string_Hashing()
            int hashed_key = string_Hashing(key_from_cql_query);
            cout << hashed_key << endl;

            //On effectue le modulo du hash (int) de la clé par le nombre de serveurs pour savoir vers lequel rediriger
            int range_id = hashed_key % server_count;
            cout << range_id << endl;

            //On détermine le serveur vers lequel rediriger
            if (range_id == server_A.server_id)
            {
                server_to_redirect = server_A;
            }
            else if (range_id == server_B.server_id)
            {
                server_to_redirect = server_B;
            }
            else if (range_id == server_C.server_id)
            {
                server_to_redirect = server_C;
            }
            else if (range_id == server_D.server_id)
            {
                server_to_redirect = server_D;
            }
            else if (range_id == server_E.server_id)
            {
                server_to_redirect = server_E;
            }
            else if (range_id == server_F.server_id)
            {
                server_to_redirect = server_F;
            }

            if (server_to_redirect.server_id != actual_server.server_id)
            {
                //Redirection
                cout << "Requete a rediriger vers " << server_to_redirect.server_name << endl;
                if (server_to_redirect.server_id > actual_server.server_id)
                    send_to_server(connected_connections[server_to_redirect.server_id - 1], req.stream, tempReq);
                else
                    send_to_server(connected_connections[server_to_redirect.server_id], req.stream, tempReq);
            }
            else
            {
                //Envoi vers PostgreSQL
                cout << "Requete a envoyer vers PostgreSQL" << endl;
                memcpy(req.request, tempReq.c_str(), tempReq.length());
                l_bufferRequestsForActualServer.push_front(req);
            }
            key_from_cql_query = "";
        }
    }

    return NULL;
}

int string_Hashing(string _key_from_cql_query)
{
    hash<string> hash_string;
    int returned_hashed_key = hash_string(_key_from_cql_query);

    if (returned_hashed_key < 0)
        returned_hashed_key = 0 - returned_hashed_key;
    return returned_hashed_key;
}

string key_extractor(string _incoming_cql_query)
{
    //TODO same as CQLTOSQL, few thinks to change
    const string select_clauses[4] = { "SELECT ", "FROM ", "WHERE ", "LIMIT " };
    const string update_clauses[3] = { "UPDATE ", "SET ", "WHERE " };
    const string insert_into_clauses[2] = { "INSERT INTO ", "VALUES " };
    const string delete_clauses[2] = { "DELETE ", "WHERE " };

    size_t where_sub_pos, limit_sub_pos, values_sub_pos = 0;

    string where_clause, values_clause = "";

    string key = "";

    if (_incoming_cql_query.substr(0, 6) == "SELECT" || _incoming_cql_query.substr(0, 6) == "select")
    {
        where_sub_pos = _incoming_cql_query.find(select_clauses[2]);
        limit_sub_pos = _incoming_cql_query.find(select_clauses[3]);

        where_clause = _incoming_cql_query.substr(where_sub_pos, limit_sub_pos - where_sub_pos);

        //On extrait, affiche et retourne la clé
        key = extract_where_data(where_clause);
        cout << "Extracted Key: " << key << endl;

        return key;
    }

    //Si requête UPDATE:
    else if (_incoming_cql_query.find("UPDATE ") != std::string::npos)
    {
        where_sub_pos = _incoming_cql_query.find(update_clauses[2]);

        where_clause = _incoming_cql_query.substr(where_sub_pos, std::string::npos);

        //On extrait, affiche et retourne la clé
        key = extract_where_data(where_clause);
        cout << "Extracted Key: " << key << endl;

        return key;
    }

    //Si requête INSERT:
    else if (_incoming_cql_query.find("INSERT ") != std::string::npos)
    {
        values_sub_pos = _incoming_cql_query.find(insert_into_clauses[1]);

        values_clause = _incoming_cql_query.substr(values_sub_pos, std::string::npos);

        //On extrait, affiche et retourne la clé
        key = extract_values_data(values_clause)[0];
        cout << "Extracted Key: " << key << endl;

        return key;
    }

    //Si requête DELETE:
    else if (_incoming_cql_query.find("DELETE ") != std::string::npos)
    {
        where_sub_pos = _incoming_cql_query.find(delete_clauses[1]);

        where_clause = _incoming_cql_query.substr(where_sub_pos, std::string::npos);

        //On extrait, affiche et retourne la clé
        key = extract_where_data(where_clause);
        cout << "Extracted Key: " << key << endl;

        return key;
    }
    else {
        logs("");
        exit(EXIT_FAILURE);
    }
}

void* Listening_socket(void* arg)
{
    char buffer[1024];
    int length;
    while (1)
    {
        for (int i = 0; i < connected_connections.size(); i++)
        {
            if (recv(connected_connections[i], buffer, sizeof(buffer), 0) > 0)
            {
                length = (buffer[7] * 256) + buffer[8] + 9;
                unsigned char u_buffer[length];
                memcpy(u_buffer, buffer, length);
                write(sockDataClient, u_buffer, sizeof(u_buffer));
                cout << "Recu d'un autre serveur et envoye a sockDataClient" << endl;

                memset(buffer, 0, 1024);
                length = 0;
            }
        }
    }
}

//ADDED
void BENCH_redirecting(PrepAndExecReq _PrepAndExecReq)
{
    string str_table_name;
    int tableNameSize = 0;
    char tableName[24];

    memcpy(tableName, &_PrepAndExecReq.CQLStatement[27], 24);
    for (tableNameSize = 0; tableNameSize < 24; tableNameSize++)
    {
        str_table_name += tableName[tableNameSize];
        if (tableName[tableNameSize] == 0x00)
            break;
    }

    //string key_from_cql_query = key_extractor(str_table_name);

    //On hash la clé extraite de la requête via la fonction string_Hashing()
    int hashed_key = string_Hashing(str_table_name);
    //cout << hashed_key << endl;

    //On effectue le modulo du hash (int) de la clé par le nombre de serveurs pour savoir vers lequel rediriger
    int range_id = hashed_key % server_count;
    //cout << range_id << endl;

    //On détermine le serveur vers lequel rediriger
    if (range_id == server_A.server_id)
    {
        server_to_redirect = server_A;
    }
    else if (range_id == server_B.server_id)
    {
        server_to_redirect = server_B;
    }
    else if (range_id == server_C.server_id)
    {
        server_to_redirect = server_C;
    }
    else if (range_id == server_D.server_id)
    {
        server_to_redirect = server_D;
    }
    else if (range_id == server_E.server_id)
    {
        server_to_redirect = server_E;
    }
    else if (range_id == server_F.server_id)
    {
        server_to_redirect = server_F;
    }

    if (server_to_redirect.server_id != actual_server.server_id)
    {
        //Redirection
        cout << "_PrepAndExecReq a rediriger vers " << server_to_redirect.server_name << endl;
        if (server_to_redirect.server_id > actual_server.server_id)
            send_to_server(connected_connections[server_to_redirect.server_id - 1], _PrepAndExecReq.head, _PrepAndExecReq.CQLStatement);
        else
            send_to_server(connected_connections[server_to_redirect.server_id], _PrepAndExecReq.head, _PrepAndExecReq.CQLStatement);
    }
    else
    {
        AddToQueue(_PrepAndExecReq);
        cout << "_PrepAndExecReq AddToQueue  depuis BENCH_redirecting" << endl;
        /*int size = (unsigned int)_PrepAndExecReq.head[7] * 256 + (unsigned int)_PrepAndExecReq.head[8];
        cout << "SIZE 7/8 before send: " << size << endl;
        cout << "_PrepAndExecReq.head: " << endl;
        for(int i = 0; i < 13; i++)
            cout << _PrepAndExecReq.head[i];
        cout << " _PrepAndExecReq.CQLStatement: " << endl;
        for(int i = 0; i < size; i++)
            cout <<  _PrepAndExecReq.CQLStatement[i];*/
    }

    //key_from_cql_query = "";
}
//ENDADDED
#pragma endregion Redirecting