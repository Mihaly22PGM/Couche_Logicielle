// #include <sys/socket.h>
// #include <sys/un.h>
// #include <netinet/in.h>
// #include <arpa/inet.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <unistd.h>
// #include <errno.h>
// #include <sys/types.h>
// #include <pthread.h>
// #include <list>
// #include <vector>
// #include <queue>
// #include <mutex>
// #include <std::string.h>
#include <algorithm>
// #include <future>
#include "c_Socket.hpp"
#include "c_Logs.hpp"
#include "c_PreparedStatements.hpp"
#include "libpq-fe.h"

//Debug libraries
// #include <iostream>
// #include <sys/stat.h>
// #include <time.h>
// #include <chrono>
// #include <fstream>
// #include <functional>       
#include <ifaddrs.h>

// using namespace std;
// using std::string;
typedef int SOCKET;

#define _NPOS std::string::npos
#define _OPTIONS_STATEMENT 0x05
#define _QUERY_STATEMENT 0x07
#define _PREPARE_STATEMENT 0x09
#define _EXECUTE_STATEMENT 0x0a
#define _THREDS_EXEC_NUMBER 7

#pragma region DeleteForProd
// std::string const nomFichier("/home/tfe_rwcs/Couche_Logicielle/Request.log");
// struct stat buffe;   
// ofstream fichier(nomFichier.c_str());
#pragma endregion DeleteForProd

#pragma region Structures

// struct Requests {
//     char opcode[1];
//     char stream[2];
//     int size;
//     char request[2048];    //TEST
//     int origin; //0 = issu du serveur, 1 = issu de la redirection
// };
struct Requests {
    unsigned char opcode[1];
    unsigned char stream[2];
    int size;
    unsigned char request[2048];    //TEST
    int origin; //0 = issu du serveur, 1 = issu de la redirection
};
// struct SQLRequests {
//     char stream[2];
//     char key_name[255];
//     int pos_key;
//     char key[255];
//     std::string request;
//     int origin; //0 = issu du serveur, 1 = issu de la redirection
// };
struct server
{
    std::string server_name;
    int server_id;
    std::string server_ip_address;
};
struct char_array{
   unsigned char chararray[131072];
};

#pragma endregion Structures

#pragma region Global
bool bl_UseReplication = false;
bool bl_UseBench = false;
bool bl_Load = false;
bool bl_Error_Args = false;
bool bl_lastRequestFrame = false;
bool bl_loop = true;
const char* conninfo = "user = postgres";
unsigned char buffData[131072];
unsigned char header[13];

Requests s_Requests;

std::queue<char_array> q_bufferFrames;
std::list<Requests> l_bufferRequests;
// std::list<SQLRequests> l_bufferPGSQLRequests;
size_t /*from_sub_pos,*/ where_sub_pos, limit_sub_pos, /*set_sub_pos,*/ values_sub_pos = 0;
// std::string select_clause, from_clause, where_clause, update_clause, set_clause, insert_into_clause, values_clause, delete_clause = "";
std::string where_clause, values_clause = "";
std::string /*table,*/ key = "";
int pos_key = 0;
std::string key_name = "";
// std::string LowerRequest = "";
std::string _incoming_cql_query = "";
// std::vector<std::string> fields, values, columns;

SOCKET sockServer;
SOCKET sockDataClient;
unsigned char UseResponse[] = { 0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x03, 0x00, 0x04, 0x79, 0x63, 0x73, 0x62 };

int socket_for_client, client_connection;
// unsigned char perm_double_zero[2] = { 0x00, 0x00 };
// unsigned char perm_column_separator[2] = { 0x00, 0x0d };
// unsigned char perm_column_value_separation[4] = { 0x00, 0x00, 0x00, 0x01 };
// unsigned char perm_null_element[4] = { 0xff, 0xff, 0xff, 0xff };

//threads
pthread_t th_FrameClient;
pthread_t th_FrameData;
pthread_t th_Requests;
pthread_t th_Redirecting;
pthread_t th_INITSocket_Redirection;
pthread_t th_PrepExec[_THREDS_EXEC_NUMBER];
std::mutex mtx_q_frames;
std::mutex mtx_q_ReqActuServer;

PGconn* conn;
PGresult* res;

int autoclose = 0;  //TO close prog after 30 secs
int server_count = 2;
server actual_server;
server neighbor_server_1;
server neighbor_server_2;
server server_to_redirect;
int port = 8042;
std::queue<Requests> l_bufferRequestsForActualServer;    //CHANGED
int socket_neighbor_1, socket_neighbor_2;

//Liste des serveurs
server server_A = { "RWCS-vServer1", 0, "192.168.82.55" };
server server_B = { "RWCS-vServer2", 1, "192.168.82.61" };
server server_C = { "RWCS-vServer3", 2, "192.168.82.63" };
server server_D = { "RWCS-vServer4", 3, "192.168.82.56" };
server server_E = { "RWCS-vServer5", 4, "192.168.82.58" };
server server_F = { "RWCS-vServer6", 5, "192.168.82.59" };

#pragma endregion Global

#pragma region Prototypes
void TraitementFrameData(/*void**/unsigned char[131072]);
//void* TraitementRequests(void*);
void* INITSocket_Redirection(void*);
void* redirecting(void*);
void closeSockets();
void closeThreads();
void exit_prog(int);
void send_to_server(int, std::string);
void server_identification();
int string_Hashing(std::string);
int connect_to_server(server, int);
std::string get_ip_from_actual_server();
std::string extract_from_data(std::string);
std::string extract_where_data(std::string);
std::string extract_key_name(std::string);
std::string extract_update_data(std::string);
std::string extract_insert_into_data_table(std::string);
std::string extract_delete_data(std::string);
std::string key_extractor(std::string);
std::vector<std::string> extract_insert_into_data_columns(std::string);
std::vector<std::string> extract_values_data(std::string);
std::vector<std::string> extract_select_data(std::string);
std::vector<std::string> extract_set_data(std::string);
#pragma endregion Prototypes

unsigned int sommeSize = 0;
bool bl_partialRequest = false;
unsigned char partialRequest[2048];
unsigned char partialHeader[13];
int sizeheader=0;
unsigned char test[131072];
unsigned char frameData[131072];
int autoIncrementRequest = 0;
std::string usereq="";
PrepAndExecReq s_PrepAndExec_ToSend;

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
    else{
        bl_Error_Args = true;
    }
    if(bl_Error_Args){
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
    if(bl_UseBench){
        for(int i=0; i<_THREDS_EXEC_NUMBER; i++){
            CheckThreadCreation += pthread_create(&th_PrepExec[i], NULL, ConnPGSQLPrepStatements, (void*)bl_Load);
        }
    }
    
    if (bl_UseReplication) {
        server_identification();
        int b = 0;
        while (b != 1)
        {
            std::cout << "tape 1 qd la redirection est lancee sur tous les serveurs" << std::endl;
            std::cin >> b;
            std::cout << std::endl;
        }
    }
    //Starting threads for sockets
    //CheckThreadCreation += pthread_create(&th_FrameData, NULL, TraitementFrameData, NULL);
    if (bl_UseReplication) {
        CheckThreadCreation += pthread_create(&th_Redirecting, NULL, redirecting, NULL);
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
    initClock(std::chrono::high_resolution_clock::now());
    timestamp("Starting Done", std::chrono::high_resolution_clock::now());
    char_array frameToSend;
    ssize_t ServerSock=0;
    while (bl_loop) {
        ServerSock = 0;
        ServerSock = recv(sockDataClient, &buffData[0], sizeof(buffData), 0);
        if (ServerSock > 0) {
            if(ServerSock > 65000){
                logs("Attention Billy ça va peter", WARNING);
                printf("%d\r\n", ServerSock);    
            }
            TraitementFrameData(buffData);
            // timestamp("Received frame", std::chrono::high_resolution_clock::now());
            // memcpy(&frameToSend.chararray[0], &buffData[0], sizeof(buffData));
            // while (!mtx_q_frames.try_lock()){}
            // q_bufferFrames.push(frameToSend);
            // mtx_q_frames.unlock();
            memset(buffData, 0, sizeof(buffData));
            // timestamp("Frame pushed", std::chrono::high_resolution_clock::now());
            autoclose=0;
        }
        else{
            autoclose++;
            if(autoclose>30000){    //Après 30 secondes, fermeture automatique du prog
                bl_loop =false;
            }
            //std::this_thread::sleep_for(std::chrono::nanoseconds(10));  //DELETE?
        }
    }
    printf("Fermeture du programme...\r\n");
    Ending();
    StopSocketThread();
    closeThreads();
    logs("Fermeture des ports...");
    closeSockets();   //TODO
    logs("Libération mémoire");
    PQfinish(conn);
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
                        if (bl_partialRequest){
                            if(partialHeader[4] == _EXECUTE_STATEMENT)
                                sizeheader = 9;
                            else
                                sizeheader = 13;
                            memcpy(&test[0], &partialHeader[0], sizeheader);
                            memcpy(&test[sizeheader], &partialRequest[0], sizeof(partialRequest)-sizeheader);                      
                            for(unsigned int i=sizeheader; i<sizeof(partialRequest); i++){
                                if(test[i] == 0x00 && test[i+1] ==0x00 && test[i+2] == 0x00 &&test[i+3] == 0x00){
                                    if(frameData[0] == 0x00){
                                        for(int j = 0; j<2;j++){
                                            if(frameData[j+1] !=0){i++;}  //If frame is cut between multiple 0x00, need always 3 0x00 to separate exec champs
                                        }
                                    }
                                    else if(frameData[0] == 0x64 && frameData[102] == 0x00) //All champs starts with 0000 000d to separate them
                                        i+=3;
                                    memcpy(&test[i], frameData, sizeof(frameData)-i);
                                    i = sizeof(partialRequest);
                                    bl_partialRequest = false;
                                }
                            }
                        }
                        else{
                            memcpy(&test[0], &frameData[0], sizeof(frameData));
                        }
                        while (!bl_lastRequestFrame && !bl_partialRequest){
                            //autoIncrementRequest++;
                            // if(sommeSize>64450){
                            //     for(int i=0; i<131072; i++){ printf("0x%x ", test[i]);}
                            //     printf("\r\n");
                            // }
                            memcpy(&header[0], &test[sommeSize], 13);
                            memcpy(&s_Requests.opcode[0], &header[4], 1);
                            memcpy(&s_Requests.stream[0], &header[2], 2);
                            s_Requests.origin = 0;                              //TODO delete?
                            if(s_Requests.opcode[0] != _EXECUTE_STATEMENT){
                                s_Requests.size = (unsigned int)header[11] * 256 + (unsigned int)header[12];
                                memcpy(&s_Requests.request[0], &test[13 + sommeSize], s_Requests.size);
                                sommeSize += s_Requests.size + 13;              //Request size + header size(13)
                            }
                            else{
                                s_Requests.size = (unsigned int)header[7] * 256 + (unsigned int)header[8];
                                memcpy(s_Requests.request, &test[9 + sommeSize], s_Requests.size);
                                sommeSize += s_Requests.size + 9;
                            }
                            if (test[sommeSize-1] == 0x00 && test[sommeSize-2] == 0x00 && test[sommeSize-3] ==0x00){          //Checking for partial request
                                bl_partialRequest = true;
                                //autoIncrementRequest--;
                            }
                            if (!bl_partialRequest){
                                switch (s_Requests.opcode[0])
                                {
                                case _QUERY_STATEMENT:
                                    if (test[sommeSize] == 0x00 && test[sommeSize + 1] == 0x01 && test[sommeSize + 2] == 0x00) {    //Checking for USE statements
                                        sommeSize = sommeSize + 3;  //TODO normally can be in USE condition
                                    }
                                    if (test[sommeSize] == 0x00)        //Checking last frame
                                        bl_lastRequestFrame = true;
                                    else if (test[sommeSize - 2] == 0x04){   //TODO useless?
                                        sommeSize = sommeSize - 2;
                                        printf("Should not pass here \r\n");
                                    }
                                    if (s_Requests.request[0] == 'U' && s_Requests.request[1] == 'S' && s_Requests.request[2] == 'E'){
                                        // usereq = std::string((char*)s_Requests.request);
                                        // if(usereq.substr(0,3) == "USE"){
                                            printf("USE\r\n");
                                        memcpy(&UseResponse[2], &s_Requests.stream, 2);
                                        write(sockDataClient, UseResponse, sizeof(UseResponse));
                                        // }
                                        // else{
                                        //     printf("Ouch\r\n");
                                        // }
                                    } 
                                    else if (bl_UseReplication)
                                        l_bufferRequests.push_front(s_Requests);
                                    else{
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
                                    AddToQueue(s_PrepAndExec_ToSend);
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
                                    AddToQueue(s_PrepAndExec_ToSend);
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
                            else{
                                logs("Partial request", WARNING);
                                memset(partialRequest, 0, sizeof(partialRequest));
                                memset(partialHeader, 0, sizeof(partialHeader));
                                memcpy(&partialRequest[0], &s_Requests.request[0], sizeof(partialRequest));
                                memcpy(&partialHeader[0], &header[0], sizeof(partialHeader));
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
std::vector<std::string> extract_values_data(std::string values_clause_data)
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
}

std::string extract_where_data(std::string _where_clause_data)
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
}
#pragma endregion CQLtoSQL

#pragma region Utils

void exit_prog(int codeEXIT) {

    logs("exit_prog() : Fin du programme...");
    exit(codeEXIT);
}

void closeSockets(){
    int closeSocketResult = 0;
    int tempSock = GetSocketConn();
    closeSocketResult += close(sockDataClient);
    closeSocketResult += close(tempSock);
    closeSocketResult += close(client_connection);
    printf("Sock close : %d\r\n", closeSocketResult);
    if(closeSocketResult == 0)
        logs("Sockets closed");
    else{
        logs("Socket closing", ERROR);
    }
    return;
}

void closeThreads(){
    for(int i=0; i<_THREDS_EXEC_NUMBER; i++){
        pthread_join(th_PrepExec[i], NULL);
    }
    pthread_join(th_Requests, NULL);
    pthread_join(th_FrameClient, NULL);
    pthread_join(th_FrameData, NULL);
    pthread_join(th_Redirecting, NULL);
    pthread_join(th_INITSocket_Redirection, NULL);
}
#pragma endregion Utils

#pragma region Listening
void* INITSocket_Redirection(void* arg)
{
    struct sockaddr_in address;
    Requests req;
    unsigned char buffer[1024];

    socket_for_client = socket(AF_INET, SOCK_STREAM, 0);
    memset(&address, 0, sizeof(address));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    bind(socket_for_client, (struct sockaddr*)&address, sizeof(address));

    listen(socket_for_client, 10);

    client_connection = accept(socket_for_client, (struct sockaddr*)NULL, NULL);

    recv(client_connection, buffer, sizeof(buffer), 0);
    memcpy(req.request, buffer, 1024);
    //ADDED
    req.origin = 1;
    //ENDADDED
    printf("Not here normally\r\n");
    l_bufferRequestsForActualServer.push(req);
    std::cout << std::endl << buffer << std::endl;

    return NULL;
}
#pragma endregion Listening

#pragma region Preparation
void server_identification()
{
    if (get_ip_from_actual_server() == server_A.server_ip_address)
    {
        actual_server = server_A;
        neighbor_server_1 = server_B;
        //neighbor_server_2 = server_F;
    }

    else if (get_ip_from_actual_server() == server_B.server_ip_address)
    {
        actual_server = server_B;
        neighbor_server_1 = server_A;
        //neighbor_server_1 = server_C;
        //neighbor_server_2 = server_A;
    }

    else if (get_ip_from_actual_server() == server_C.server_ip_address)
    {
        actual_server = server_C;
        neighbor_server_1 = server_D;
        neighbor_server_2 = server_B;
    }

    else if (get_ip_from_actual_server() == server_D.server_ip_address)
    {
        actual_server = server_D;
        neighbor_server_1 = server_E;
        neighbor_server_2 = server_C;
    }

    else if (get_ip_from_actual_server() == server_E.server_ip_address)
    {
        actual_server = server_E;
        neighbor_server_1 = server_F;
        neighbor_server_2 = server_D;
    }

    else if (get_ip_from_actual_server() == server_F.server_ip_address)
    {
        actual_server = server_F;
        neighbor_server_1 = server_A;
        neighbor_server_2 = server_E;
    }

    std::cout << get_ip_from_actual_server() << std::endl;
    std::cout << "Serveur #" << actual_server.server_id << ", Nom : " << actual_server.server_name << ", Adresse IP: " << actual_server.server_ip_address << std::endl;

    //On crée les connexions permanentes avec les serveurs voisins
    socket_neighbor_1 = connect_to_server(neighbor_server_1, port);               //...
    //socket_neighbor_2 = connect_to_server(neighbor_server_2, port);
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
    logs("connect_to_server() : Connexion établie avec " + _server_to_connect.server_ip_address);
    // std::cout << std::endl << "Connexion etablie avec " << ip_address << std::endl;

    return sock_to_server;
}

void send_to_server(int _socketServer, std::string _query_to_send)
{
    const char* cql_query = _query_to_send.c_str();

    send(_socketServer, cql_query, strlen(cql_query), 0);
    std::cout << std::endl << "Incoming query sent" << std::endl;
}
#pragma endregion Server_connection

#pragma region Redirecting
void* redirecting(void* arg)
{
    Requests req;
    std::string tempReq;
    // char stream[2];
    while (bl_loop)
    {
        //On hash la clé extraite de la requête via la fonction string_Hashing()
        if (l_bufferRequests.size() > 0)
        {
            tempReq = std::string((char*) l_bufferRequests.back().request);
            memcpy(req.stream, l_bufferRequests.back().stream, 2);
            memcpy(req.opcode, l_bufferRequests.back().opcode, 1);
            // req.RequestNumber=stream;
            // req.RequestOpcode='0x07';
            l_bufferRequests.pop_back();
            std::cout << "redirecting() pop back ok" << std::endl;
            std::string key_from_cql_query = key_extractor(tempReq);

            int hashed_key = string_Hashing(key_from_cql_query);
            std::cout << hashed_key << std::endl;

            //On détermine le serveur vers lequel rediriger
            int range_id = hashed_key % server_count;
            std::cout << range_id << std::endl;

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

            //On effectue le modulo du hash (int) de la clé par le nombre de serveurs pour savoir vers lequel rediriger
            if (server_to_redirect.server_id != actual_server.server_id)
            {
                //On regarde si on doit rediriger vers un voisin
                if (server_to_redirect.server_id == neighbor_server_1.server_id)              //...
                {
                    std::cout << "Requete a rediriger vers le voisin " << server_to_redirect.server_name << std::endl;
                    send_to_server(socket_neighbor_1, tempReq);
                }
                //Si non on crée la connection et on envoie
                else
                {
                    std::cout << "Requete a rediriger vers " << server_to_redirect.server_name << std::endl;
                    send_to_server(connect_to_server(server_to_redirect, port), tempReq);
                }
            }
            else
            {
                
                //Envoi vers PostgreSQL
                // req.Request=tempReq.c_str();
                memcpy(req.request, tempReq.c_str(), tempReq.length());
                printf("Not here normally\r\n");
                l_bufferRequestsForActualServer.push(req);
                std::cout << "Requete a envoyer vers PostgreSQL" << std::endl;
            }
            key_from_cql_query = "";
        }
    }
    logs("Redirecting() : Fin du thread");
    pthread_exit(NULL);
    return NULL;
}

int string_Hashing(std::string _key_from_cql_query)
{
    std::hash<std::string> hash_string;
    int returned_hashed_key = hash_string(_key_from_cql_query);

    if (returned_hashed_key < 0)
        returned_hashed_key = 0 - returned_hashed_key;
    return returned_hashed_key;
}

std::string key_extractor(std::string _incoming_cql_query)
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
        std::cout << "Extracted Key: " << key << std::endl;

        return key;
    }

    //Si requête UPDATE:
    else if (_incoming_cql_query.find("UPDATE ") != std::string::npos)
    {
        where_sub_pos = _incoming_cql_query.find(update_clauses[2]);

        where_clause = _incoming_cql_query.substr(where_sub_pos, std::string::npos);

        //On extrait, affiche et retourne la clé
        key = extract_where_data(where_clause);
        std::cout << "Extracted Key: " << key << std::endl;

        return key;
    }

    //Si requête INSERT:
    else if (_incoming_cql_query.find("INSERT ") != std::string::npos)
    {
        values_sub_pos = _incoming_cql_query.find(insert_into_clauses[1]);

        values_clause = _incoming_cql_query.substr(values_sub_pos, std::string::npos);

        //On extrait, affiche et retourne la clé
        key = extract_values_data(values_clause)[0];
        std::cout << "Extracted Key: " << key << std::endl;

        return key;
    }

    //Si requête DELETE:
    else if (_incoming_cql_query.find("DELETE ") != std::string::npos)
    {
        where_sub_pos = _incoming_cql_query.find(delete_clauses[1]);

        where_clause = _incoming_cql_query.substr(where_sub_pos, std::string::npos);

        //On extrait, affiche et retourne la clé
        key = extract_where_data(where_clause);
        std::cout << "Extracted Key: " << key << std::endl;

        return key;
    }
    else {
        logs("");
        exit(EXIT_FAILURE);
    }
}
#pragma endregion Redirecting