#include <algorithm>
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
#define _THREDS_EXEC_NUMBER 6
#define _REDIRECTING_PORT 8042
#define _CONNEXIONS 10
#define _WARNING_BUFFER 100000

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
bool bl_UseRedirection = false;
bool bl_UseBench = false;
bool bl_Load = false;
bool bl_Error_Args = false;
bool bl_loop = true;
bool bl_SlaveMaster = false;
bool bl_WithSlave = false;
bool bl_partialRequest = false;
bool bl_lastRequestFrame = false;
bool bl_partialRequest_REPL[_CONNEXIONS];
bool bl_lastRequestFrame_REPL = false;

int CheckThreadCreation = 0;
unsigned int sommeSize = 0;
unsigned int sommeSize_REPL = 0;
unsigned int sizeheader = 0;
unsigned int sizeheader_REPL = 0;
unsigned int server_count = 0;
unsigned int master_count = 0;
unsigned int datatransfert = 0;
unsigned int datatransfert_REPL[_CONNEXIONS];

ssize_t ServerSock = 0;

unsigned char buffData[131072];
unsigned char test[131072];
unsigned char PrevTest[131072];
unsigned char header[13];
unsigned char frameData[131072];
unsigned char partialRequest[2048];
unsigned char partialHeader[13];
unsigned char test_REPL[131072];
unsigned char header_REPL[13];
unsigned char frameData_REPL[131072];
unsigned char partialRequest_REPL[_CONNEXIONS][2048];
unsigned char partialHeader_REPL[_CONNEXIONS][13];
unsigned char UseResponse[19] = { 0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x03, 0x00, 0x04, 0x79, 0x63, 0x73, 0x62 };     //Automatic response for USE requests
unsigned char ResponseExecute[13] = { 0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01 };                                     //Prototype for Execute requests
unsigned char UseResponse_REPL[19] = { 0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x03, 0x00, 0x04, 0x79, 0x63, 0x73, 0x62 };     //Automatic response for USE requests
unsigned char ResponseExecute_REPL[13] = { 0x84, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01 };                                     //Prototype for Execute requests

//Sockets - Expand to int 
SOCKET sockServer;
SOCKET sockDataClient;
SOCKET socket_for_client, client_connection;

//Threads
pthread_t th_INITSocket_Redirection;
pthread_t th_PrepExec[_THREDS_EXEC_NUMBER];
pthread_t th_Listening_socket;

//Structures
Requests s_Requests;
Requests s_Requests_REPL;

PrepAndExecReq s_PrepAndExec_ToSend;
PrepAndExecReq s_PrepAndExec_ToSend_REPL;

server actual_server;
server server_to_redirect;
//Server List
server server_A = { "RWCS_vServer1", 0, "192.168.82.52" };
server server_B = { "RWCS_vServer2", 1, "192.168.82.53" };
//server server_C = { "RWCS_vServer3", 2, "192.168.82.59" };
/*server server_D = { "RWCS_vServer4", 3, "192.168.82.56" };
server server_E = { "RWCS_vServer5", 4, "192.168.82.58" };
server server_F = { "RWCS_vServer6", 5, "192.168.82.59" };*/

std::vector<int> accepted_connections, connected_connections;

//Important de respecter l'ordre des id quand on déclare les sevreurs dans la liste pour que ça coincide avec la position dans la liste (METTRE LES MASTERS AU DEBUT)
std::vector<server> l_servers = { server_A, server_B/*, server_C, server_D, server_E, server_F */ };

#pragma endregion Global

#pragma region Prototypes
void TraitementFrameData(unsigned char[131072]);
void closeSockets();
void closeThreads();
void exit_prog(int);
void server_identification();
void send_to_server(int, unsigned char[13], unsigned char[2048]);
void BENCH_redirecting(PrepAndExecReq);

int string_Hashing(std::string);
int connect_to_server(server, int);

std::string get_ip_from_actual_server();

void* Listening_socket(void*);
void* INITSocket_Redirection(void*);
#pragma endregion Prototypes

int main(int argc, char* argv[])
{
    //Initialization
    server_count = l_servers.size();
    master_count = server_count;
    memset(&PrevTest[0], 0, sizeof(PrevTest));

    for (unsigned int i = 0; i < _CONNEXIONS; i++) {
        bl_partialRequest_REPL[i] = false;
        datatransfert_REPL[i] = 0;
    }
    bl_Load = true;     //Load mode forced
    bl_UseBench = true; //bench mode forced

    if (argc == 4) {    //3 arguments
        if (std::string(argv[1]) == "master")
            bl_SlaveMaster = true;
        else if (std::string(argv[1]) != "slave")
            bl_Error_Args = true;
        if (std::string(argv[2]) == "red")
            bl_UseRedirection = true;
        else if (std::string(argv[2]) != "alone")
            bl_Error_Args = true;
        if (std::string(argv[3]) == "sl")
            bl_WithSlave = true;
        else if (std::string(argv[3]) != "nosl")
            bl_Error_Args = true;
    }
    else
        bl_Error_Args = true;
    if (bl_Error_Args) {
        printf("Incorrect Parameters : ./CoucheLogicielle [master|slave] [red|alone] [sl|nosl]\r\n");
        exit_prog(EXIT_FAILURE);
    }
    if (bl_WithSlave) {
        if (bl_SlaveMaster)
            master_count = server_count - 1;
    }
    else {
        if (bl_SlaveMaster)
            master_count = server_count;
    }

    if (bl_UseRedirection) {
        if (bl_SlaveMaster) {
            logs("main() : Starting Proxy...Replication mode selected");
            CheckThreadCreation += pthread_create(&th_INITSocket_Redirection, NULL, INITSocket_Redirection, NULL);  //1 Thread creation
            if (CheckThreadCreation != 0) {
                logs("main() : Thread th_INITSocket_Redirection creation failed", ERROR);
                exit_prog(EXIT_FAILURE);
            }
            else
                logs("main() : Thread th_INITSocket_Redirection created");
        }
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

    if (bl_UseRedirection) {
        server_identification();

        int b = 0;
        while (b != 1)
        {
            std::cout << "tape 1 qd la redirection est lancee sur tous les serveurs" << std::endl;
            std::cin >> b;
            std::cout << std::endl;
        }
    }
    if (bl_UseBench && bl_SlaveMaster) {
        for (int i = 0; i < _THREDS_EXEC_NUMBER; i++) {
            CheckThreadCreation += pthread_create(&th_PrepExec[i], NULL, ConnPGSQLPrepStatements, NULL);    //_THREDS_EXEC_NUMBER Threads creation
        }
    }
    else if (!bl_SlaveMaster)
        CheckThreadCreation += pthread_create(&th_PrepExec[1], NULL, ConnPGSQLPrepStatements, NULL);    //1 Thread creation

    //Starting threads for sockets
    if (bl_UseRedirection) {
        CheckThreadCreation += pthread_create(&th_Listening_socket, NULL, Listening_socket, NULL);  //1 Thread creation
    }

    //Check if threads have been created
    if (CheckThreadCreation != 0) {
        logs("main() : Error while creating threads", ERROR);
        exit_prog(EXIT_FAILURE);
    }
    else
        logs("main() : Threads creation success");

    //Réception des frames en continu et mise en buffer
    sockServer = CreateSocket();
    sockDataClient = INITSocket(sockServer, bl_UseBench);

    logs("main() : Starting Done");
    while (bl_loop) {
        ServerSock = 0;
        ServerSock = recv(sockDataClient, &buffData[0], sizeof(buffData), 0);
        if (ServerSock > 0) {
            if (ServerSock > _WARNING_BUFFER) {
                logs("Buffer full! Ou presque!", WARNING);
                printf("Check if not too much data : 0x%x 0x%x 0x%x 0x%x", buffData[131068], buffData[131069], buffData[131070], buffData[131071]);
            }
            TraitementFrameData(buffData);
            memset(buffData, 0, sizeof(buffData));
        }
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
    printf("Fermeture du programme...\r\n");
    Ending();
    StopSocketThread();
    closeThreads();
    logs("Fermeture des ports...");
    closeSockets();
    logs("Libération mémoire");
    printf("Fin du programme...\r\n");
    logs("Fin du programme");
    return EXIT_SUCCESS;
}

#pragma region Requests
void TraitementFrameData(unsigned char buffofdata[131072]) {
    memset(&test[0], 0, sizeof(test));
    memset(&header[0], 0, sizeof(header));
    sommeSize = 0;
    bl_lastRequestFrame = false;
    if (bl_partialRequest) {
        if (ServerSock + datatransfert + sizeheader > _WARNING_BUFFER) {
            logs("Buffer quasi full!!", WARNING);
        }
        if (partialHeader[4] == _EXECUTE_STATEMENT)
            sizeheader = 9;
        else if (partialHeader[4] == _PREPARE_STATEMENT)
            sizeheader = 13;
        else {
            logs("Casseee", WARNING);
            memcpy(&test[0], &buffofdata[0], ServerSock);
        }
        memcpy(&test[0], &partialHeader[0], sizeheader);
        memcpy(&test[sizeheader], &partialRequest[0], datatransfert);
        memcpy(&test[datatransfert + sizeheader], &buffofdata[0], ServerSock);
        ServerSock += datatransfert + sizeheader;
        bl_partialRequest = false;
    }
    else {
        memcpy(&test[0], &buffofdata[0], ServerSock);     //COPY frame if previous frame is not partial
    }
    while (!bl_lastRequestFrame && !bl_partialRequest) {
        if (sommeSize > _WARNING_BUFFER)
            logs("Buffer quasi full attention!!", WARNING);
        memcpy(&header[0], &test[sommeSize], 13);
        memcpy(&s_Requests.opcode[0], &header[4], 1);
        memcpy(&s_Requests.stream[0], &header[2], 2);
        s_Requests.origin = sockDataClient;
        if (s_Requests.opcode[0] != _EXECUTE_STATEMENT) {
            s_Requests.size = (unsigned int)header[11] * 256 + (unsigned int)header[12];
            memcpy(&s_Requests.request[0], &test[13 + sommeSize], s_Requests.size);
            sommeSize += s_Requests.size + 13;
        }
        else {
            s_Requests.size = (unsigned int)header[7] * 256 + (unsigned int)header[8];
            memcpy(s_Requests.request, &test[9 + sommeSize], s_Requests.size);
            sommeSize += s_Requests.size + 9;
        }
        if (sommeSize > ServerSock)
            bl_partialRequest = true;
        if (!bl_partialRequest) {
            switch (s_Requests.opcode[0])
            {
            case _QUERY_STATEMENT:
                if (test[sommeSize] == 0x00 && test[sommeSize + 1] == 0x01 && test[sommeSize + 2] == 0x00) {    //Checking for USE statements
                    sommeSize = sommeSize + 3;
                }
                if (test[sommeSize] == 0x00)        //Checking last frame
                    bl_lastRequestFrame = true;
                else if (test[sommeSize - 2] == 0x04) {   //TODO useless?
                    sommeSize = sommeSize - 2;
                    logs("Si erreur look here?", WARNING);
                    printf("Should not pass here \r\n");
                }
                if (s_Requests.request[0] == 'U' && s_Requests.request[1] == 'S' && s_Requests.request[2] == 'E') {
                    memcpy(&UseResponse[2], &s_Requests.stream, 2);
                    write(sockDataClient, UseResponse, sizeof(UseResponse));
                }
                else {
                    printf("Ce message signifie que ça pue la merde ton programme Billy\r\n");
                }
                break;
            case _EXECUTE_STATEMENT:
                memcpy(s_PrepAndExec_ToSend.head, header, sizeof(header));
                memcpy(s_PrepAndExec_ToSend.CQLStatement, s_Requests.request, sizeof(s_Requests.request));
                s_PrepAndExec_ToSend.origin = sockDataClient;
                if (bl_UseRedirection) {
                    BENCH_redirecting(s_PrepAndExec_ToSend);
                }
                else {
                    AddToQueue(s_PrepAndExec_ToSend);
                }
                memset(s_PrepAndExec_ToSend.head, 0, sizeof(s_PrepAndExec_ToSend.head));
                memset(s_PrepAndExec_ToSend.CQLStatement, 0, sizeof(s_PrepAndExec_ToSend.CQLStatement));
                if (sommeSize == ServerSock) {        //Checking last frame
                    bl_lastRequestFrame = true;
                }
                break;
            case _PREPARE_STATEMENT:
                if (sommeSize == ServerSock)        //Checking last frame
                    bl_lastRequestFrame = true;
                memcpy(s_PrepAndExec_ToSend.head, header, sizeof(header));
                memcpy(s_PrepAndExec_ToSend.CQLStatement, s_Requests.request, sizeof(s_Requests.request));
                s_PrepAndExec_ToSend.origin = sockDataClient;
                AddToQueue(s_PrepAndExec_ToSend);
                break;
            case _OPTIONS_STATEMENT:
                logs("DO THIS FUCKING ISALIVE REQUEST FRANZICHE", WARNING);
                break;
            default:
                bl_lastRequestFrame = true;
                printf("Pas bon ça\r\n");
                logs("TraitementFrameData() : Type of request unknown : " + std::string((char*)s_Requests.request), ERROR);
                break;
            }
        }
        else {
            if (bl_partialRequest) {
                datatransfert = s_Requests.size - (sommeSize - ServerSock);  //Size diff excluding header size
                memset(partialRequest, 0, sizeof(partialRequest));
                memset(partialHeader, 0, sizeof(partialHeader));
                if (s_Requests.request[datatransfert + 1] != 0x0 || datatransfert > 2047) {
                    logs("Erreur ici?\r\n");
                    printf("Datatransfert %d\r\n", datatransfert);
                    printf("char : %c\r\n", s_Requests.request[datatransfert + 1]);
                }
                memcpy(&partialRequest[0], &s_Requests.request[0], datatransfert);
                memcpy(&partialHeader[0], &header[0], sizeof(partialHeader));
            }
        }//Fin de requête
        memset(&s_Requests.request[0], 0, sizeof(s_Requests.request));
    }//Fin de frame
    memcpy(&PrevTest[0], &test[0], sizeof(PrevTest));
    memset(&test[0], 0x00, sizeof(test));
    memset(&header[0], 0x00, sizeof(header));
}

#pragma endregion Requests

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
    pthread_join(th_INITSocket_Redirection, NULL);
}
#pragma endregion Utils

#pragma region Listening
void* INITSocket_Redirection(void* arg)
{
    struct sockaddr_in address;
    unsigned char buffer[_CONNEXIONS][131072];
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
    logs("Connexion des serveurs effectuée");
    ssize_t ByteSize_REPL = 0;
    while (bl_loop)
    {
        std::this_thread::sleep_for(std::chrono::microseconds(1));
        for (unsigned int zz = 0; zz < accepted_connections.size(); zz++)   //foreach connexion
        {
            ByteSize_REPL = recv(accepted_connections[zz], &buffer[zz][0], sizeof(buffer[zz]), 0);
            if (ByteSize_REPL > 0)
            {
                if (ByteSize_REPL > _WARNING_BUFFER) {
                    logs("Buffer_REPL full?", WARNING);
                    printf("Check if not too much data : 0x%x 0x%x 0x%x 0x%x", buffer[zz][131068], buffer[zz][131069], buffer[zz][131070], buffer[zz][131071]);
                }
                memset(&test_REPL[0], 0, sizeof(test_REPL));
                memset(&header_REPL[0], 0, sizeof(header_REPL));
                sommeSize_REPL = 0;
                bl_lastRequestFrame_REPL = false;
                if (bl_partialRequest_REPL[zz]) {
                    if (partialHeader_REPL[zz][4] == _EXECUTE_STATEMENT)
                        sizeheader_REPL = 9;
                    else if (partialHeader_REPL[zz][4] == _PREPARE_STATEMENT)
                        sizeheader_REPL = 13;
                    else {
                        logs("Ouch", WARNING);
                        memcpy(&test_REPL[0], &buffer[zz][0], ByteSize_REPL);
                    }
                    memcpy(&test_REPL[0], &partialHeader_REPL[zz][0], sizeheader_REPL);
                    memcpy(&test_REPL[sizeheader_REPL], &partialRequest_REPL[zz][0], datatransfert_REPL[zz]);
                    memcpy(&test_REPL[datatransfert_REPL[zz] + sizeheader_REPL], &buffer[zz][0], ByteSize_REPL);
                    ByteSize_REPL += datatransfert_REPL[zz] + sizeheader_REPL;
                    bl_partialRequest_REPL[zz] = false;
                }
                else {
                    memcpy(&test_REPL[0], &buffer[zz][0], ByteSize_REPL);
                }
                memset(&buffer[zz][0], 0, ByteSize_REPL);
                while (!bl_lastRequestFrame_REPL && !bl_partialRequest_REPL[zz]) {
                    if (sommeSize_REPL > _WARNING_BUFFER) {
                        logs("Warning indiquant l'éventualité que TOUT RISQUE DE PETER!!!", WARNING);
                    }
                    memcpy(&header_REPL[0], &test_REPL[sommeSize_REPL], 13);
                    memcpy(&s_Requests_REPL.opcode[0], &header_REPL[4], 1);
                    memcpy(&s_Requests_REPL.stream[0], &header_REPL[2], 2);
                    s_Requests_REPL.origin = accepted_connections[zz];
                    if (s_Requests_REPL.opcode[0] != _EXECUTE_STATEMENT) {
                        s_Requests_REPL.size = (unsigned int)header_REPL[11] * 256 + (unsigned int)header_REPL[12];     //size request
                        memcpy(&s_Requests_REPL.request[0], &test_REPL[13 + sommeSize_REPL], s_Requests_REPL.size);
                        sommeSize_REPL += s_Requests_REPL.size + 13;
                    }
                    else {
                        s_Requests_REPL.size = (unsigned int)header_REPL[7] * 256 + (unsigned int)header_REPL[8];       //size request
                        memcpy(s_Requests_REPL.request, &test_REPL[9 + sommeSize_REPL], s_Requests_REPL.size);
                        sommeSize_REPL += s_Requests_REPL.size + 9;
                    }
                    if (sommeSize_REPL > ByteSize_REPL) {
                        bl_partialRequest_REPL[zz] = true;
                    }
                    if (!bl_partialRequest_REPL[zz]) {
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
                                memcpy(&UseResponse_REPL[2], &s_Requests_REPL.stream, 2);
                                write(accepted_connections[zz], UseResponse_REPL, sizeof(UseResponse_REPL));
                            }
                            else {
                                logs("Ce message signifie que ça pue la merde ton programme Billy\r\n", WARNING);
                            }
                            break;
                        case _EXECUTE_STATEMENT:
                            memcpy(s_PrepAndExec_ToSend_REPL.head, header_REPL, sizeof(header_REPL));
                            memcpy(s_PrepAndExec_ToSend_REPL.CQLStatement, s_Requests_REPL.request, sizeof(s_Requests_REPL.request));
                            s_PrepAndExec_ToSend_REPL.origin = accepted_connections[zz];
                            AddToQueue(s_PrepAndExec_ToSend_REPL);
                            memset(s_PrepAndExec_ToSend_REPL.head, 0, sizeof(s_PrepAndExec_ToSend_REPL.head));
                            memset(s_PrepAndExec_ToSend_REPL.CQLStatement, 0, sizeof(s_PrepAndExec_ToSend_REPL.CQLStatement));
                            if (sommeSize_REPL == ByteSize_REPL)        //Checking last frame
                                bl_lastRequestFrame_REPL = true;
                            break;
                        case _PREPARE_STATEMENT:
                            if (sommeSize_REPL == ByteSize_REPL)        //Checking last frame
                                bl_lastRequestFrame_REPL = true;
                            memcpy(s_PrepAndExec_ToSend_REPL.head, header_REPL, sizeof(header_REPL));
                            memcpy(s_PrepAndExec_ToSend_REPL.CQLStatement, s_Requests_REPL.request, sizeof(s_Requests_REPL.request));
                            s_PrepAndExec_ToSend_REPL.origin = accepted_connections[zz];
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

                    }
                    else {
                        datatransfert_REPL[zz] = s_Requests_REPL.size - (sommeSize_REPL - ByteSize_REPL);  //Size diff excluding header size
                        memset(partialRequest_REPL, 0, sizeof(partialRequest_REPL));
                        memset(partialHeader_REPL[zz], 0, sizeof(partialHeader_REPL[zz]));
                        if (s_Requests_REPL.request[datatransfert_REPL[zz] + 1] != 0x0 || datatransfert_REPL[zz] > 2047) {
                            logs("Erreur in datatransfert_REPL?", WARNING);
                            logs("Erreur ici?\r\n");
                            printf("Datatransfert %d\r\n", datatransfert_REPL[zz]);
                            printf("char : %c\r\n", s_Requests_REPL.request[datatransfert_REPL[zz] + 1]);
                        }
                        memcpy(&partialRequest_REPL[0], &s_Requests_REPL.request[0], datatransfert_REPL[zz]);
                        memcpy(&partialHeader_REPL[zz][0], &header_REPL[0], sizeof(partialHeader_REPL[zz]));
                    }//Fin de requête

                    memset(&s_Requests_REPL.request[0], 0, sizeof(s_Requests_REPL.request));

                }//Fin de frame
                memset(&test_REPL[0], 0x0, sizeof(test_REPL));
                memset(&header_REPL[0], 0x0, sizeof(header_REPL));
            }
        }
    }
    return NULL;
}
#pragma endregion Listening

#pragma region Preparation
void server_identification()
{
    for (unsigned int i = 0; i < master_count; i++)
    {
        if (get_ip_from_actual_server() == l_servers[i].server_ip_address)
        {
            actual_server = l_servers[i];

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

    return sock_to_server;
}

void send_to_server(int _socketServer, unsigned char _head[13], unsigned char _CQLStatement[2048])
{
    int size = (unsigned int)_head[7] * 256 + (unsigned int)_head[8];
    unsigned char cql_query[9 + size];
    memset(cql_query, 0, sizeof(cql_query));

    memcpy(cql_query, _head, 9);
    memcpy(cql_query + 9, _CQLStatement, size);

    write(_socketServer, cql_query, sizeof(cql_query));
}
#pragma endregion Server_connection

#pragma region Redirecting

int string_Hashing(std::string _key_from_cql_query)
{
    std::hash<std::string> hash_string;
    int returned_hashed_key = hash_string(_key_from_cql_query);

    if (returned_hashed_key < 0)
        returned_hashed_key = 0 - returned_hashed_key;
    return returned_hashed_key;
}

void* Listening_socket(void* arg)
{
    unsigned char buffer[131072];
    int bytes_received = 0;
    while (1)
    {
        for (unsigned int i = 0; i < connected_connections.size(); i++)
        {
            bytes_received = recv(connected_connections[i], buffer, sizeof(buffer), 0);
            if (bytes_received > 0)
            {
                write(sockDataClient, buffer, bytes_received);
                memset(buffer, 0, bytes_received);
            }
            std::this_thread::sleep_for(std::chrono::microseconds(1));
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

    //On hash la clé extraite de la requête via la fonction string_Hashing()
    int hashed_key = string_Hashing(str_table_name);

    //On effectue le modulo du hash (int) de la clé par le nombre de serveurs pour savoir vers lequel rediriger
    int range_id = hashed_key % (server_count /*- 1*/);

    //On détermine le serveur vers lequel rediriger
    if (range_id == server_A.server_id)
    {
        server_to_redirect = server_A;
    }
    else if (range_id == server_B.server_id)
    {
        server_to_redirect = server_B;
    }
    else {
        logs("Noooon", ERROR);
    }

    if (server_to_redirect.server_id != actual_server.server_id)
    {
        //Redirection
        if (server_to_redirect.server_id > actual_server.server_id)
            send_to_server(connected_connections[server_to_redirect.server_id - 1], _PrepAndExecReq.head, _PrepAndExecReq.CQLStatement);
        else
            send_to_server(connected_connections[server_to_redirect.server_id], _PrepAndExecReq.head, _PrepAndExecReq.CQLStatement);
    }
    else
    {
        AddToQueue(_PrepAndExecReq);
    }

}
#pragma endregion Redirecting