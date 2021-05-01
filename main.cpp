#include <algorithm>
#include <fcntl.h>
#include "c_Socket.hpp"
#include "c_Logs.hpp"
#include "c_PreparedStatements.hpp"
#include "libpq-fe.h"    
#include <ifaddrs.h>

typedef int SOCKET;

#define _NPOS std::string::npos
#define _OPTIONS_STATEMENT 0x05
#define _QUERY_STATEMENT 0x07
#define _PREPARE_STATEMENT 0x09
#define _EXECUTE_STATEMENT 0x0a
#define _THREDS_EXEC_NUMBER 4
#define _REDIRECTING_PORT 8042

#pragma region Structures

struct Requests {
    unsigned char opcode[1];
    unsigned char stream[2];
    int size;
    unsigned char request[2048];
    int origin; //sockDataClient = issu du serveur, accepted_connections[i] = issu de la redirection
};

struct char_array {
    unsigned char chararray[131072];
};

#pragma endregion Structures

#pragma region Global
bool bl_UseReplication = false;
bool bl_UseBench = false;
bool bl_Load = false;
bool bl_Error_Args = false;
bool bl_loop = true;

unsigned char buffData[131072];

//TODO Move this in Global Region if Possible
bool bl_partialRequest = false;
bool bl_lastRequestFrame = false;
unsigned int sommeSize = 0;
int sizeheader = 0;
unsigned char test[131072];
unsigned char header[13];
unsigned char frameData[131072];
unsigned char partialRequest[2048];
unsigned char partialHeader[13];
Requests s_Requests;
PrepAndExecReq s_PrepAndExec_ToSend;
unsigned char UseResponse[] = { 0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x03, 0x00, 0x04, 0x79, 0x63, 0x73, 0x62 };     //Automatic response for USE requests
unsigned char ResponseExecute[13] = { 0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01 };                                     //Prototype for Execute requests
//int autoIncrementRequest = 0;

//ADDED
bool bl_partialRequest_REPL = false;
bool bl_lastRequestFrame_REPL = false;
unsigned int sommeSize_REPL = 0;
int sizeheader_REPL = 0;
unsigned char test_REPL[131072];
unsigned char header_REPL[13];
unsigned char frameData_REPL[131072];
unsigned char partialRequest_REPL[2048];
unsigned char partialHeader_REPL[13];
Requests s_Requests_REPL;
PrepAndExecReq s_PrepAndExec_ToSend_REPL;
unsigned char UseResponse_REPL[] = { 0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x03, 0x00, 0x04, 0x79, 0x63, 0x73, 0x62 };     //Automatic response for USE requests
unsigned char ResponseExecute_REPL[13] = { 0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01 };                                     //Prototype for Execute requests

//int autoIncrementRequest = 0;
//ENDADDED

replication_relation actual_and_subscriber;

// std::queue<char_array> q_bufferFrames;
// std::list<Requests> l_bufferRequests;
// size_t where_sub_pos, limit_sub_pos, values_sub_pos = 0;
// std::string where_clause, values_clause = "";
// std::string key = "";
// int pos_key = 0;
// std::string key_name = "";
// std::string _incoming_cql_query = "";
// std::string usereq = "";

//Sockets - Expand to int 
SOCKET sockServer;
SOCKET sockDataClient;
SOCKET socket_for_client, client_connection;

//Threads
pthread_t th_FrameClient;
// pthread_t th_FrameData;
// pthread_t th_Requests;
// pthread_t th_Redirecting;
pthread_t th_INITSocket_Redirection;
pthread_t th_PrepExec[_THREDS_EXEC_NUMBER];
pthread_t th_Listening_socket;

//Mutex
// std::mutex mtx_q_frames;
// std::mutex mtx_q_ReqActuServer;

//PostgreSQL Variables
// PGconn* conn;
// PGresult* res;
// const char* conninfo = "user = postgres";

server actual_server;
server subscriber_server;
// server neighbor_server_1;
// server neighbor_server_2;
server server_to_redirect;
// std::queue<Requests> l_bufferRequestsForActualServer;
// int socket_neighbor_1/*, socket_neighbor_2*/;

std::vector<int> accepted_connections, connected_connections;
//Server List
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
server server_E = { "RWCS_vServer5", 4, "192.168.82.63" };
server server_F = { "RWCS_vServer6", 5, "192.168.82.64" };
//Important de respecter l'ordre des id quand on déclare les sevreurs dans la liste pour que ça coincide avec la position dans la liste
std::vector<server> l_servers = { server_A, server_B/*, server_C, server_D, server_E, server_F */ };
const unsigned int server_count = l_servers.size();

#pragma endregion Global

#pragma region Prototypes
void TraitementFrameData(unsigned char[131072]);
void* INITSocket_Redirection(void*);
// void* redirecting(void*);
void closeSockets();
void closeThreads();
void exit_prog(int);
// void send_to_server(int, std::string);
void server_identification();
int string_Hashing(std::string);
int connect_to_server(server, int);
std::string get_ip_from_actual_server();
/* std::string extract_from_data(std::string);
std::string extract_where_data(std::string);
std::string extract_key_name(std::string);
std::string extract_update_data(std::string);
std::string extract_insert_into_data_table(std::string);
std::string extract_delete_data(std::string);
std::string key_extractor(std::string);
std::vector<std::string> extract_insert_into_data_columns(std::string);
std::vector<std::string> extract_values_data(std::string);
std::vector<std::string> extract_select_data(std::string);
std::vector<std::string> extract_set_data(std::string);*/
void* Listening_socket(void*);
void send_to_server(int, unsigned char[13], unsigned char[2048]);
void BENCH_redirecting(PrepAndExecReq);
#pragma endregion Prototypes

int main(int argc, char* argv[])
{
    bl_Load = true;     //Load mode forced
    bl_UseBench = true; //bench mode forced
    if (argc == 2) {    //1 arguments
        if (std::string(argv[1]) == "repl")
            bl_UseReplication = true;
        else if (std::string(argv[1]) != "alone")
            bl_Error_Args = true;
    }
    else {
        bl_Error_Args = true;
    }
    if (bl_Error_Args) {
        printf("Incorrect Parameters : ./CoucheLogicielle [repl|alone]\r\n");
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
            std::cout << "tape 1 qd la couche logicielle est lancee sur tous les serveurs" << std::endl;
            std::cin >> a;
            std::cout << std::endl;
        }
    }
    else
        logs("main() : Starting Proxy...Standalone mode selected");

    if (bl_UseReplication) {
        server_identification();
        actual_and_subscriber = { actual_server, subscriber_server/*, 0 */ };   // REPLICATION FACTOR
        std::cout << "Serveur actual_server: " << actual_server.server_id << " | " << actual_server.server_ip_address << " | " << actual_server.server_name << std::endl;
        std::cout << "Serveur subscriber_server: " << subscriber_server.server_id << " | " << subscriber_server.server_ip_address << " | " << subscriber_server.server_name << std::endl;
        int b = 0;
        while (b != 1)
        {
            std::cout << "tape 1 qd la redirection est lancee sur tous les serveurs" << std::endl;
            std::cin >> b;
            std::cout << std::endl;
        }
    }
    if (bl_UseBench) {
        for (int i = 0; i < _THREDS_EXEC_NUMBER; i++) {
            // CheckThreadCreation += pthread_create(&th_PrepExec[i], NULL, ConnPGSQLPrepStatements, (void*)bl_Load);
            if (bl_UseReplication) {
                // actual_and_subscriber.th_num = i;
                CheckThreadCreation += pthread_create(&th_PrepExec[i], NULL, ConnPGSQLPrepStatements, (void*)&actual_and_subscriber);
            }
            else
                CheckThreadCreation += pthread_create(&th_PrepExec[i], NULL, ConnPGSQLPrepStatements, NULL);
        }
    }
    //Starting threads for sockets
    //CheckThreadCreation += pthread_create(&th_FrameData, NULL, TraitementFrameData, NULL);
    if (bl_UseReplication) {
        // CheckThreadCreation += pthread_create(&th_Redirecting, NULL, redirecting, NULL);
        CheckThreadCreation += pthread_create(&th_Listening_socket, NULL, Listening_socket, NULL);
    }
    //Check if threads have been created
    if (CheckThreadCreation != 0) {
        logs("main() : Error while creating threads", ERROR);
        exit_prog(EXIT_FAILURE);
    }

    //Réception des frames en continu et mise en buffer
    sockServer = CreateSocket();
    sockDataClient = INITSocket(sockServer, bl_UseBench);
    CheckThreadCreation += pthread_create(&th_FrameClient, NULL, TraitementFrameClient, NULL);

    if (CheckThreadCreation != 0) {
        logs("main() : Error while creating threads", ERROR);
        exit_prog(EXIT_FAILURE);
    }
    else
        logs("main() : Threads creation success");
    logs("main() : Starting Done");
    //initClock(std::chrono::high_resolution_clock::now());
    //timestamp("Starting Done", std::chrono::high_resolution_clock::now());
    ssize_t ServerSock = 0;
    while (bl_loop) {
        ServerSock = 0;
        ServerSock = recv(sockDataClient, &buffData[0], sizeof(buffData), 0);
        if (ServerSock > 0) {
            TraitementFrameData(buffData);
            memset(buffData, 0, sizeof(buffData));
        }
    }
    printf("Fermeture du programme...\r\n");
    Ending();
    StopSocketThread();
    closeThreads();
    logs("Fermeture des ports...");
    closeSockets();   //TODO
    logs("Libération mémoire");
    // PQfinish(conn);
    printf("Fin du programme...\r\n");
    logs("Fin du programme");
    return EXIT_SUCCESS;
}

#pragma region Requests
//void* TraitementFrameData(void* arg) {
void TraitementFrameData(unsigned char buffofdata[131072]) {
    // unsigned int sommeSize = 0;
    //bl_partialRequest = false;
    // unsigned char partialRequest[2048];
    // unsigned char partialHeader[13];
    // int sizeheader=0;
    // unsigned char test[131072];
    // unsigned char frameData[131072];
    // int autoIncrementRequest = 0;
    // std::string usereq="";
    // PrepAndExecReq s_PrepAndExec_ToSend;
    try {
        memset(&test[0], 0, sizeof(test));
        memset(&header[0], 0, sizeof(header));
        //while (bl_loop) {
            // if (!q_bufferFrames.empty()){
            //     if(mtx_q_frames.try_lock()){
                    // if(q_bufferFrames.size()>0){
                        // memcpy(&frameData[0], &q_bufferFrames.front().chararray[0], sizeof(frameData));
        memcpy(&frameData[0], &buffofdata[0], sizeof(frameData));
        memset(&buffofdata[0], 0, 131072);
        // q_bufferFrames.pop();
        // mtx_q_frames.unlock();
        sommeSize = 0;
        bl_lastRequestFrame = false;
        if (bl_partialRequest) {
            if (partialHeader[4] == _EXECUTE_STATEMENT)
                sizeheader = 9;
            else
                sizeheader = 13;
            memcpy(&test[0], &partialHeader[0], sizeheader);
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
            memcpy(&test[0], &frameData[0], sizeof(frameData));
        }
        while (!bl_lastRequestFrame && !bl_partialRequest) {
            //autoIncrementRequest++;
            // if(sommeSize>64450){
            //     for(int i=0; i<131072; i++){ printf("0x%x ", test[i]);}
            //     printf("\r\n");
            // }
            memcpy(&header[0], &test[sommeSize], 13);
            memcpy(&s_Requests.opcode[0], &header[4], 1);
            memcpy(&s_Requests.stream[0], &header[2], 2);
            s_Requests.origin = sockDataClient;                              //TODO delete?
            if (s_Requests.opcode[0] != _EXECUTE_STATEMENT) {
                s_Requests.size = (unsigned int)header[11] * 256 + (unsigned int)header[12];
                memcpy(&s_Requests.request[0], &test[13 + sommeSize], s_Requests.size);
                sommeSize += s_Requests.size + 13;              //Request size + header size(13)
            }
            else {
                s_Requests.size = (unsigned int)header[7] * 256 + (unsigned int)header[8];
                memcpy(s_Requests.request, &test[9 + sommeSize], s_Requests.size);
                sommeSize += s_Requests.size + 9;
            }
            if (test[sommeSize - 1] == 0x00 && test[sommeSize - 2] == 0x00 && test[sommeSize - 3] == 0x00) {          //Checking for partial request
                bl_partialRequest = true;
                //autoIncrementRequest--;
            }
            if (!bl_partialRequest) {
                switch (s_Requests.opcode[0])
                {
                case _QUERY_STATEMENT:
                    if (test[sommeSize] == 0x00 && test[sommeSize + 1] == 0x01 && test[sommeSize + 2] == 0x00) {    //Checking for USE statements
                        sommeSize = sommeSize + 3;  //TODO normally can be in USE condition
                    }
                    if (test[sommeSize] == 0x00)        //Checking last frame
                        bl_lastRequestFrame = true;
                    else if (test[sommeSize - 2] == 0x04) {   //TODO useless?
                        sommeSize = sommeSize - 2;
                        printf("Should not pass here \r\n");
                    }
                    if (s_Requests.request[0] == 'U' && s_Requests.request[1] == 'S' && s_Requests.request[2] == 'E') {
                        // usereq = std::string((char*)s_Requests.request);
                        // if(usereq.substr(0,3) == "USE"){
                        //printf("USE\r\n");
                        memcpy(&UseResponse[2], &s_Requests.stream, 2);
                        write(sockDataClient, UseResponse, sizeof(UseResponse));
                        // }
                        // else{
                        //     printf("Ouch\r\n");
                        // }
                    }
                    /* else if (bl_UseReplication)
                        l_bufferRequests.push_front(s_Requests);*/
                    else {
                        printf("Ce message signifie que ça pue la merde ton programme Billy\r\n");
                    }
                    // else{
                    //     while (!mtx_q_ReqActuServer.try_lock()){}
                    //     printf("Should not pass here\r\n");
                    //     l_bufferRequestsForActualServer.push(s_Requests);
                    //     mtx_q_ReqActuServer.unlock();
                    // }
                    break;
                case _EXECUTE_STATEMENT:
                    memcpy(s_PrepAndExec_ToSend.head, header, sizeof(header));
                    memcpy(s_PrepAndExec_ToSend.CQLStatement, s_Requests.request, sizeof(s_Requests.request));
                    s_PrepAndExec_ToSend.origin = sockDataClient;
                    if (bl_UseReplication) {
                        BENCH_redirecting(s_PrepAndExec_ToSend);
                        //      std::cout << "_EXECUTE_STATEMENT envoye a BENCH_redirecting depuis le client" << std::endl;
                    }
                    else {
                        AddToQueue(s_PrepAndExec_ToSend);
                        //    std::cout << "_EXECUTE_STATEMENT AddToQueue depuis le client" << std::endl;
                    }
                    memset(s_PrepAndExec_ToSend.head, 0, sizeof(s_PrepAndExec_ToSend.head));
                    memset(s_PrepAndExec_ToSend.CQLStatement, 0, sizeof(s_PrepAndExec_ToSend.CQLStatement));
                    if (test[sommeSize] != 0x04)        //Checking last frame
                        bl_lastRequestFrame = true;
                    break;
                case _PREPARE_STATEMENT:
                    if (test[sommeSize] != 0x04)        //Checking last frame
                        bl_lastRequestFrame = true;
                    memcpy(s_PrepAndExec_ToSend.head, header, sizeof(header));
                    memcpy(s_PrepAndExec_ToSend.CQLStatement, s_Requests.request, sizeof(s_Requests.request));
                    s_PrepAndExec_ToSend.origin = sockDataClient;
                    AddToQueue(s_PrepAndExec_ToSend);
                    //std::cout << "_PREPARE_STATEMENT AddToQueue depuis le client" << std::endl;
                    break;
                case _OPTIONS_STATEMENT:
                    logs("DO THIS FUCKING ISALIVE REQUEST FRANZICHE", WARNING);
                    break;
                default:
                    bl_lastRequestFrame = true;
                    logs("TraitementFrameData() : Type of request unknown : " + std::string((char*)s_Requests.request), ERROR);
                    break;
                }
                // if (fichier) {
                //     fichier << "Requete N° " << autoIncrementRequest << ", Taille : " << s_Requests.size << " : " << s_Requests.request << std::endl;
                // }      
            }
            else {
                if (s_Requests.opcode[0] == 0x0a) {
                    //printf("0x%x 0x%x 0x%x 0x%x\r\n", s_Requests.request[91], s_Requests.request[92], s_Requests.request[93], s_Requests.request[94]);
                    // printf("0x%x 0x%x 0x%x 0x%x\r\n", s_Requests.request[1], s_Requests.request[2], s_Requests.request[3], s_Requests.request[4]);
                    // for(int i=50; i<150; i++){
                    //         printf("0x%x ", s_Requests.request[i]);
                    //     }
                    //                                             printf("\r\n");

                    if (s_Requests.request[91] == 0x13 || s_Requests.request[91] == 0x88 || s_Requests.request[92] == 0x05 || s_Requests.request[101] == 0xc0) {
                        printf("Mouais\r\n");
                        bl_partialRequest = false;
                        memcpy(&ResponseExecute[2], &s_Requests.stream[0], 2);
                        write(GetSocket(), ResponseExecute, sizeof(ResponseExecute));
                        for (int i = 50; i < 150; i++) {
                            printf("0x%x ", s_Requests.request[i]);
                        }
                        printf("\r\n");
                    }
                }
                if (bl_partialRequest) {
                    logs("Partial request", WARNING);
                    memset(partialRequest, 0, sizeof(partialRequest));
                    memset(partialHeader, 0, sizeof(partialHeader));
                    memcpy(&partialRequest[0], &s_Requests.request[0], sizeof(partialRequest));
                    memcpy(&partialHeader[0], &header[0], sizeof(partialHeader));
                }
                // if(partialRequest[0] == 0x00 && partialRequest[1] == 0x00 && partialRequest[2] == 0x00 && partialRequest[3] == 0x00)
                //     bl_partialRequest = false;
            }//Fin de requête
            memset(&s_Requests.request[0], 0, sizeof(s_Requests.request));
            // if(sommeSize>65000){
            //     bl_lastRequestFrame=true;
            // }
        }//Fin de frame
        memset(&test[0], 0x00, sizeof(test));
        memset(&header[0], 0x00, sizeof(header));
        // timestamp("Frame OK", std::chrono::high_resolution_clock::now());
    // }
    // else{
    //     mtx_q_frames.unlock();
    //     //std::this_thread::sleep_for(std::chrono::milliseconds(1));
    // }
// }
// else{
//     std::this_thread::sleep_for(std::chrono::nanoseconds(10)); //TODO maybe need to edit
// }
//     }
//     // else{
//     //     std::this_thread::sleep_for(std::chrono::nanoseconds(10)); //TODO maybe need to edit
//     // }
// }
    }
    catch (std::exception const& e) {
        logs("TraitementRequests() : " + std::string(e.what()), ERROR);
    }
    //logs("TraitementRequests() : Stopping thread...");
    //pthread_exit(NULL);
}

#pragma endregion Requests

#pragma region CQLtoSQL
/*std::vector<std::string> extract_values_data(std::string values_clause_data)
{
    values_clause_data = values_clause_data.substr(6, _NPOS);
    std::vector<std::string> returned_vector;

    size_t pos = 0;
    std::string token;

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
}*/

/*std::string extract_where_data(std::string _where_clause_data)
{
    std::string where_clause_data = _where_clause_data;
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
}*/
#pragma endregion CQLtoSQL

#pragma region Utils

void exit_prog(int codeEXIT) {

    logs("exit_prog() : Fin du programme...");
    exit(codeEXIT);
}

void closeSockets() {
    int closeSocketResult = 0;
    int tempSock = GetSocketConn();
    closeSocketResult += close(sockDataClient);
    closeSocketResult += close(tempSock);
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
    for (int i = 0; i < _THREDS_EXEC_NUMBER; i++) {
        pthread_join(th_PrepExec[i], NULL);
    }
    // pthread_join(th_Requests, NULL);
    pthread_join(th_FrameClient, NULL);
    // pthread_join(th_FrameData, NULL);
    // pthread_join(th_Redirecting, NULL);
    pthread_join(th_INITSocket_Redirection, NULL);
}
#pragma endregion Utils

#pragma region Listening
void* INITSocket_Redirection(void* arg)
{
    struct sockaddr_in address;
    //Requests req;
    unsigned char buffer[131072];

    socket_for_client = socket(AF_INET, SOCK_STREAM, 0);
    memset(&address, 0, sizeof(address));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(_REDIRECTING_PORT);

    bind(socket_for_client, (struct sockaddr*)&address, sizeof(address));

    listen(socket_for_client, 10);

    while (accepted_connections.size() < server_count - 1)
    {
        client_connection = accept(socket_for_client, (struct sockaddr*)NULL, NULL);
        fcntl(client_connection, F_SETFL, O_NONBLOCK);
        accepted_connections.push_back(client_connection);
        client_connection = 0;
        std::cout << "Connexion " << accepted_connections.size() << " acceptee pour la redirection" << std::endl;
    }

    std::cout << "Connexion a tous les serveurs acceptees!" << std::endl;

    while (bl_loop)
    {
        for (unsigned int i = 0; i < accepted_connections.size(); i++)
        {
            if (recv(accepted_connections[i], buffer, sizeof(buffer), 0) > 0)
            {
                try {
                    //std::cout << "_PrepAndExecReq reçue dans INITSocket_Redirection" << std::endl;
                    memset(&test_REPL[0], 0, sizeof(test_REPL));
                    memset(&header_REPL[0], 0, sizeof(header_REPL));
                    //while (bl_loop) {
                        // if (!q_bufferFrames.empty()){
                        //     if(mtx_q_frames.try_lock()){
                                // if(q_bufferFrames.size()>0){
                                    // memcpy(&frameData_REPL[0], &q_bufferFrames.front().chararray[0], sizeof(frameData_REPL));
                    memcpy(&frameData_REPL[0], &buffer[0], sizeof(buffer));
                    memset(&buffer[0], 0, sizeof(buffer));
                    // q_bufferFrames.pop();
                    // mtx_q_frames.unlock();
                    sommeSize_REPL = 0;
                    bl_lastRequestFrame_REPL = false;
                    if (bl_partialRequest_REPL) {
                        if (partialHeader_REPL[4] == _EXECUTE_STATEMENT)
                            sizeheader_REPL = 9;
                        else
                            sizeheader_REPL = 13;
                        memcpy(&test_REPL[0], &partialHeader_REPL[0], sizeheader_REPL);
                        memcpy(&test_REPL[sizeheader_REPL], &partialRequest_REPL[0], sizeof(partialRequest_REPL) - sizeheader_REPL);
                        for (unsigned int i = sizeheader_REPL; i < sizeof(partialRequest_REPL); i++) {
                            if (test_REPL[i] == 0x00 && test_REPL[i + 1] == 0x00 && test_REPL[i + 2] == 0x00 && test_REPL[i + 3] == 0x00) {
                                if (frameData_REPL[0] == 0x00) {
                                    for (int j = 0; j < 2; j++) {
                                        if (frameData_REPL[j + 1] != 0) { i++; }  //If frame is cut between multiple 0x00, need always 3 0x00 to separate exec champs
                                    }
                                }
                                else if (frameData_REPL[0] == 0x64 && frameData_REPL[102] == 0x00) //All champs starts with 0000 000d to separate them
                                    i += 3;
                                memcpy(&test_REPL[i], frameData_REPL, sizeof(frameData_REPL) - i);
                                i = sizeof(partialRequest_REPL);
                                bl_partialRequest_REPL = false;
                            }
                        }
                    }
                    else {
                        memcpy(&test_REPL[0], &frameData_REPL[0], sizeof(frameData_REPL));
                    }
                    //std::cout << "TT VA BIEN0" << std::endl;
                    while (!bl_lastRequestFrame_REPL && !bl_partialRequest_REPL) {
                        //autoIncrementRequest++;
                        // if(sommeSize_REPL>64450){
                        //     for(int i=0; i<131072; i++){ printf("0x%x ", test_REPL[i]);}
                        //     printf("\r\n");
                        // }
                        memcpy(&header_REPL[0], &test_REPL[sommeSize_REPL], 13);
                        memcpy(&s_Requests_REPL.opcode[0], &header_REPL[4], 1);
                        memcpy(&s_Requests_REPL.stream[0], &header_REPL[2], 2);
                        s_Requests_REPL.origin = accepted_connections[i];                              //TODO delete?
                        if (s_Requests_REPL.opcode[0] != _EXECUTE_STATEMENT) {
                            s_Requests_REPL.size = (unsigned int)header_REPL[11] * 256 + (unsigned int)header_REPL[12];
                            memcpy(&s_Requests_REPL.request[0], &test_REPL[13 + sommeSize_REPL], s_Requests_REPL.size);
                            sommeSize_REPL += s_Requests_REPL.size + 13;              //Request size + header_REPL size(13)
                        }
                        else {
                            s_Requests_REPL.size = (unsigned int)header_REPL[7] * 256 + (unsigned int)header_REPL[8];
                            memcpy(s_Requests_REPL.request, &test_REPL[9 + sommeSize_REPL], s_Requests_REPL.size);
                            sommeSize_REPL += s_Requests_REPL.size + 9;
                        }
                        if (test_REPL[sommeSize_REPL - 1] == 0x00 && test_REPL[sommeSize_REPL - 2] == 0x00 && test_REPL[sommeSize_REPL - 3] == 0x00) {          //Checking for partial request
                            bl_partialRequest_REPL = true;
                            //autoIncrementRequest--;
                        }
                        //std::cout << "TT VA BIEN1" << std::endl;
                        if (!bl_partialRequest_REPL) {
                            switch (s_Requests_REPL.opcode[0])
                            {
                            case _QUERY_STATEMENT:
                                if (test_REPL[sommeSize_REPL] == 0x00 && test_REPL[sommeSize_REPL + 1] == 0x01 && test_REPL[sommeSize_REPL + 2] == 0x00) {    //Checking for USE statements
                                    sommeSize_REPL = sommeSize_REPL + 3;  //TODO normally can be in USE condition
                                }
                                if (test_REPL[sommeSize_REPL] == 0x00)        //Checking last frame
                                    bl_lastRequestFrame_REPL = true;
                                else if (test_REPL[sommeSize_REPL - 2] == 0x04) {   //TODO useless?
                                    sommeSize_REPL = sommeSize_REPL - 2;
                                    printf("Should not pass here \r\n");
                                }
                                if (s_Requests_REPL.request[0] == 'U' && s_Requests_REPL.request[1] == 'S' && s_Requests_REPL.request[2] == 'E') {
                                    // usereq = std::string((char*)s_Requests_REPL.request);
                                    // if(usereq.substr(0,3) == "USE"){
                                    //printf("USE\r\n");
                                    memcpy(&UseResponse_REPL[2], &s_Requests_REPL.stream, 2);
                                    write(accepted_connections[i], UseResponse_REPL, sizeof(UseResponse_REPL));
                                    // }
                                    // else{
                                    //     printf("Ouch\r\n");
                                    // }
                                }
                                /*else if (bl_UseReplication)
                                    l_bufferRequests.push_front(s_Requests_REPL);*/
                                else {
                                    printf("Ce message signifie que ça pue la merde ton programme Billy\r\n");
                                }
                                // else{
                                //     while (!mtx_q_ReqActuServer.try_lock()){}
                                //     printf("Should not pass here\r\n");
                                //     l_bufferRequestsForActualServer.push(s_Requests_REPL);
                                //     mtx_q_ReqActuServer.unlock();
                                // }
                                break;
                            case _EXECUTE_STATEMENT:
                                memcpy(s_PrepAndExec_ToSend_REPL.head, header_REPL, sizeof(header_REPL));
                                memcpy(s_PrepAndExec_ToSend_REPL.CQLStatement, s_Requests_REPL.request, sizeof(s_Requests_REPL.request));
                                s_PrepAndExec_ToSend_REPL.origin = accepted_connections[i];
                                AddToQueue(s_PrepAndExec_ToSend_REPL);
                                //std::cout << "s_Requests_REPL.size: " << std::endl;
                                //std::cout << s_Requests_REPL.size << std::endl;
                                //std::cout << "s_PrepAndExec_ToSend_REPL.head: " << std::endl;
                                //for (int i = 0; i < 13; i++)
                                 //   std::cout << s_PrepAndExec_ToSend_REPL.head[i];
                                //std::cout << " s_PrepAndExec_ToSend_REPL.CQLStatement: " << std::endl;
                                //for (int i = 0; i < 2048; i++)
                                //    std::cout << s_PrepAndExec_ToSend_REPL.CQLStatement[i];
                                //std::cout << "_EXECUTE_STATEMENT AddToQueue depuis INITSocket_Redirection" << std::endl;
                                memset(s_PrepAndExec_ToSend_REPL.head, 0, sizeof(s_PrepAndExec_ToSend_REPL.head));
                                memset(s_PrepAndExec_ToSend_REPL.CQLStatement, 0, sizeof(s_PrepAndExec_ToSend_REPL.CQLStatement));
                                if (test_REPL[sommeSize_REPL] != 0x04)        //Checking last frame
                                    bl_lastRequestFrame_REPL = true;
                                break;
                            case _PREPARE_STATEMENT:
                                if (test_REPL[sommeSize_REPL] != 0x04)        //Checking last frame
                                    bl_lastRequestFrame_REPL = true;
                                memcpy(s_PrepAndExec_ToSend_REPL.head, header_REPL, sizeof(header_REPL));
                                memcpy(s_PrepAndExec_ToSend_REPL.CQLStatement, s_Requests_REPL.request, sizeof(s_Requests_REPL.request));
                                s_PrepAndExec_ToSend_REPL.origin = accepted_connections[i];
                                //std::cout << "_PREPARE_STATEMENT AddToQueue depuis INITSocket_Redirection" << std::endl;
                                AddToQueue(s_PrepAndExec_ToSend_REPL);
                                break;
                            case _OPTIONS_STATEMENT:
                                logs("DO THIS FUCKING ISALIVE REQUEST FRANZICHE", WARNING);
                                break;
                            default:
                                bl_lastRequestFrame_REPL = true;
                                logs("TraitementFrameData() : Type of request unknown : " + std::string((char*)s_Requests_REPL.request), ERROR);
                                break;
                            }
                            // if (fichier) {
                            //     fichier << "Requete N° " << autoIncrementRequest << ", Taille : " << s_Requests_REPL.size << " : " << s_Requests_REPL.request << std::endl;
                            // }      
                        }
                        else {
                            if (s_Requests_REPL.opcode[0] == 0x0a) {
                                //printf("0x%x 0x%x 0x%x 0x%x\r\n", s_Requests_REPL.request[91], s_Requests_REPL.request[92], s_Requests_REPL.request[93], s_Requests_REPL.request[94]);
                                // printf("0x%x 0x%x 0x%x 0x%x\r\n", s_Requests_REPL.request[1], s_Requests_REPL.request[2], s_Requests_REPL.request[3], s_Requests_REPL.request[4]);
                                // for(int i=50; i<150; i++){
                                //         printf("0x%x ", s_Requests_REPL.request[i]);
                                //     }
                                //                                             printf("\r\n");

                                if (s_Requests_REPL.request[91] == 0x13 || s_Requests_REPL.request[91] == 0x88 || s_Requests_REPL.request[92] == 0x05 || s_Requests_REPL.request[101] == 0xc0) {
                                    //printf("Mouais\r\n");
                                    bl_partialRequest_REPL = false;
                                    memcpy(&ResponseExecute_REPL[2], &s_Requests_REPL.stream[0], 2);
                                    write(GetSocket(), ResponseExecute_REPL, sizeof(ResponseExecute_REPL));
                                    /*for (int i = 50; i < 150; i++) {
                                        printf("0x%x ", s_Requests_REPL.request[i]);
                                    }
                                    printf("\r\n");*/
                                }
                            }
                            if (bl_partialRequest_REPL) {
                                //logs("Partial request", WARNING);
                                memset(partialRequest_REPL, 0, sizeof(partialRequest_REPL));
                                memset(partialHeader_REPL, 0, sizeof(partialHeader_REPL));
                                memcpy(&partialRequest_REPL[0], &s_Requests_REPL.request[0], sizeof(partialRequest_REPL));
                                memcpy(&partialHeader_REPL[0], &header_REPL[0], sizeof(partialHeader_REPL));
                            }
                            // if(partialRequest_REPL[0] == 0x00 && partialRequest_REPL[1] == 0x00 && partialRequest_REPL[2] == 0x00 && partialRequest_REPL[3] == 0x00)
                            //     bl_partialRequest_REPL = false;
                        }//Fin de requête
                        memset(&s_Requests_REPL.request[0], 0, sizeof(s_Requests_REPL.request));
                        // if(sommeSize_REPL>65000){
                        //     bl_lastRequestFrame=true;
                        // }
                    }//Fin de frame
                    //std::cout << "TT VA BIEN2" << std::endl;
                    memset(&test_REPL[0], 0x00, sizeof(test_REPL));
                    memset(&header_REPL[0], 0x00, sizeof(header_REPL));
                    // timestamp("Frame OK", std::chrono::high_resolution_clock::now());
                // }
                // else{
                //     mtx_q_frames.unlock();
                //     //std::this_thread::sleep_for(std::chrono::milliseconds(1));
                // }
            // }
            // else{
            //     std::this_thread::sleep_for(std::chrono::nanoseconds(10)); //TODO maybe need to edit
            // }
    //     }
    //     // else{
    //     //     std::this_thread::sleep_for(std::chrono::nanoseconds(10)); //TODO maybe need to edit
    //     // }
    // }
                }
                catch (std::exception const& e) {
                    logs("TraitementRequests() : " + std::string(e.what()), ERROR);
                }
            }
        }
    }

    return NULL;
}
#pragma endregion Listening

#pragma region Preparation
void server_identification()
{
    for (unsigned int i = 0; i < l_servers.size(); i++)
    {
        if (get_ip_from_actual_server() == l_servers[i].server_ip_address)
        {
            actual_server = l_servers[i];
            subscriber_server = l_servers[(i + 1) % l_servers.size()];

            // REPLICATION FACTOR

            std::cout << get_ip_from_actual_server() << std::endl;
            std::cout << "Serveur #" << actual_server.server_id << ", Nom : " << actual_server.server_name << ", Adresse IP: " << actual_server.server_ip_address << std::endl;
        }
        else
        {
            connected_connections.push_back(connect_to_server(l_servers[i], _REDIRECTING_PORT));
            std::cout << "Tentative de connexion avec " << l_servers[i].server_name << " (" << l_servers[i].server_ip_address << ") " << std::endl;
        }
    }
}

std::string get_ip_from_actual_server() {
    struct ifaddrs* ifAddrStruct = NULL;
    struct ifaddrs* ifa = NULL;
    void* tmpAddrPtr = NULL;

    std::string address;

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

/* void send_to_server(int _socketServer, unsigned char _stream_to_send[2], std::string _query_to_send)
{
    unsigned char cql_query[13 + _query_to_send.length()];
    unsigned char header_to_send[13] = { 0x04, 0x00, _stream_to_send[0], _stream_to_send[1], 0x07, 0x00, 0x00, (unsigned char)((_query_to_send.length() + 21) / 256), (unsigned char)(_query_to_send.length() + 21), 0x00, 0x00, (unsigned char)(_query_to_send.length() / 256), (unsigned char)(_query_to_send.length()) };

    memcpy(cql_query, header_to_send, 13);
    memcpy(cql_query + 13, _query_to_send.c_str(), _query_to_send.length());

    write(_socketServer, cql_query, 13 + _query_to_send.length());
    //std::cout << std::endl << "Incoming query sent" << std::endl;
}*/

void send_to_server(int _socketServer, unsigned char _head[13], unsigned char _CQLStatement[2048])
{
    int size = (unsigned int)_head[7] * 256 + (unsigned int)_head[8];
    //std::cout << "SIZE 7/8 at send: " << size << std::endl;
    unsigned char cql_query[9 + size];
    memset(cql_query, 0, sizeof(cql_query));

    memcpy(cql_query, _head, 9);
    memcpy(cql_query + 9, _CQLStatement, size);

    write(_socketServer, cql_query, sizeof(cql_query));

    //std::cout << "_PrepAndExecReq.head: " << std::endl;
    //for (int i = 0; i < 9; i++)
      //  std::cout << cql_query[i];
    //std::cout << " _PrepAndExecReq.CQLStatement: " << std::endl;
    //for (int i = 0; i < size; i++)
      //  std::cout << cql_query[9 + i];
    //std::cout << std::endl << "PrepAndExecReq write depuis BENCH_redirecting" << std::endl;
}
#pragma endregion Server_connection

#pragma region Redirecting
/* void* redirecting(void* arg)
{
    Requests req;
    std::string tempReq;
    while (1)
    {
        if (l_bufferRequests.size() > 0)
        {
            tempReq = (char*)l_bufferRequests.back().request;
            req.origin = l_bufferRequests.back().origin;
            memcpy(req.stream, l_bufferRequests.back().stream, 2);
            memcpy(req.opcode, l_bufferRequests.back().opcode, 1);
            l_bufferRequests.pop_back();
            //std::cout << "redirecting() pop back ok" << std::endl;

            std::string key_from_cql_query = key_extractor(tempReq);

            //On hash la clé extraite de la requête via la fonction string_Hashing()
            int hashed_key = string_Hashing(key_from_cql_query);
            //std::cout << hashed_key << std::endl;

            //On effectue le modulo du hash (int) de la clé par le nombre de serveurs pour savoir vers lequel rediriger
            int range_id = hashed_key % server_count;
//            std::cout << range_id << std::endl;

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
                //std::cout << "Requete a rediriger vers " << server_to_redirect.server_name << std::endl;
                if (server_to_redirect.server_id > actual_server.server_id)
                    send_to_server(connected_connections[server_to_redirect.server_id - 1], req.stream, tempReq);
                else
                    send_to_server(connected_connections[server_to_redirect.server_id], req.stream, tempReq);
            }
            else
            {
                //Envoi vers PostgreSQL
                //std::cout << "Requete a envoyer vers PostgreSQL" << std::endl;
                memcpy(req.request, tempReq.c_str(), tempReq.length());
                l_bufferRequestsForActualServer.push(req);
            }
            key_from_cql_query = "";
        }
    }

    return NULL;
}*/

int string_Hashing(std::string _key_from_cql_query)
{
    std::hash<std::string> hash_string;
    int returned_hashed_key = hash_string(_key_from_cql_query);

    if (returned_hashed_key < 0)
        returned_hashed_key = 0 - returned_hashed_key;
    return returned_hashed_key;
}

/* std::string key_extractor(std::string _incoming_cql_query)
{
    //TODO same as CQLTOSQL, few thinks to change
    const std::string select_clauses[4] = { "SELECT ", "FROM ", "WHERE ", "LIMIT " };
    const std::string update_clauses[3] = { "UPDATE ", "SET ", "WHERE " };
    const std::string insert_into_clauses[2] = { "INSERT INTO ", "VALUES " };
    const std::string delete_clauses[2] = { "DELETE ", "WHERE " };

    size_t where_sub_pos, limit_sub_pos, values_sub_pos = 0;

    std::string where_clause, values_clause = "";

    std::string key = "";

    if (_incoming_cql_query.substr(0, 6) == "SELECT" || _incoming_cql_query.substr(0, 6) == "select")
    {
        where_sub_pos = _incoming_cql_query.find(select_clauses[2]);
        limit_sub_pos = _incoming_cql_query.find(select_clauses[3]);

        where_clause = _incoming_cql_query.substr(where_sub_pos, limit_sub_pos - where_sub_pos);

        //On extrait, affiche et retourne la clé
        key = extract_where_data(where_clause);
        //std::cout << "Extracted Key: " << key << std::endl;

        return key;
    }

    //Si requête UPDATE:
    else if (_incoming_cql_query.find("UPDATE ") != std::string::npos)
    {
        where_sub_pos = _incoming_cql_query.find(update_clauses[2]);

        where_clause = _incoming_cql_query.substr(where_sub_pos, std::string::npos);

        //On extrait, affiche et retourne la clé
        key = extract_where_data(where_clause);
        //std::cout << "Extracted Key: " << key << std::endl;

        return key;
    }

    //Si requête INSERT:
    else if (_incoming_cql_query.find("INSERT ") != std::string::npos)
    {
        values_sub_pos = _incoming_cql_query.find(insert_into_clauses[1]);

        values_clause = _incoming_cql_query.substr(values_sub_pos, std::string::npos);

        //On extrait, affiche et retourne la clé
        key = extract_values_data(values_clause)[0];
        //std::cout << "Extracted Key: " << key << std::endl;

        return key;
    }

    //Si requête DELETE:
    else if (_incoming_cql_query.find("DELETE ") != std::string::npos)
    {
        where_sub_pos = _incoming_cql_query.find(delete_clauses[1]);

        where_clause = _incoming_cql_query.substr(where_sub_pos, std::string::npos);

        //On extrait, affiche et retourne la clé
        key = extract_where_data(where_clause);
        //std::cout << "Extracted Key: " << key << std::endl;

        return key;
    }
    else {
        logs("");
        exit(EXIT_FAILURE);
    }
}*/

void* Listening_socket(void* arg)
{
    char buffer[10240];
    int bytes_received;
    // unsigned int length = 0;
    while (bl_loop)
    {
        for (unsigned int i = 0; i < connected_connections.size(); i++)
        {
            bytes_received = recv(connected_connections[i], buffer, sizeof(buffer), 0);
            if (bytes_received > 0)
            {
                /*while(bl_loop)
                {
                    length += (buffer[7+length] * 256) + buffer[8+length] + 9;
                    if(length> sizeof(buffer)-1 || buffer[length] == 0x00)
                        break;
                }*/
                write(sockDataClient, buffer, /*length*/bytes_received);

                // std::cout << "Recu d'un autre serveur et envoye a sockDataClient" << std::endl;

                memset(buffer, 0, bytes_received);
                // length = 0;
            }
        }
    }
    return NULL;
}

void BENCH_redirecting(PrepAndExecReq _PrepAndExecReq)
{
    std::string str_table_name;
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
        //std::cout << "_PrepAndExecReq a rediriger vers " << server_to_redirect.server_name << std::endl;
        if (server_to_redirect.server_id > actual_server.server_id)
            send_to_server(connected_connections[server_to_redirect.server_id - 1], _PrepAndExecReq.head, _PrepAndExecReq.CQLStatement);
        else
            send_to_server(connected_connections[server_to_redirect.server_id], _PrepAndExecReq.head, _PrepAndExecReq.CQLStatement);
    }
    else
    {
        AddToQueue(_PrepAndExecReq);
        //std::cout << "_PrepAndExecReq AddToQueue  depuis BENCH_redirecting" << std::endl;
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
#pragma endregion Redirecting