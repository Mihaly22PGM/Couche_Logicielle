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
#include <string.h>
#include <algorithm>
#include <future>
#include "c_Socket.hpp"
#include "c_Logs.hpp"
#include "c_PreparedStatements.hpp"
#include "libpq-fe.h"

//Debug libraries
#include <iostream>
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

typedef int SOCKET;

#pragma region DeleteForProd
string const nomFichier("/home/tfe_rwcs/Couche_Logicielle/Request.log");
ofstream fichier(nomFichier.c_str());
#pragma endregion DeleteForProd

#pragma region Structures

struct Requests {
    char opcode[1];
    char stream[2];
    int size;
    char request[1024];
    //ADDED
    int origin; //0 = issu du serveur, 1 = issu de la redirection
    //ENDADDED
};
struct SQLRequests {
    char stream[2];
    char key_name[255];
    int pos_key;
    char key[255];
    string request;
    //ADDED
    int origin; //0 = issu du serveur, 1 = issu de la redirection
    //ENDADDED
};

struct server
{
    string server_name;
    int server_id;
    string server_ip_address;
};

#pragma endregion Structures

#pragma region Global
bool bl_UseReplication = false;
bool bl_lastRequestFrame = false;
const char* conninfo;
char buffData[10240];
unsigned char header[13];

Requests s_Requests;

std::list<char*> l_bufferFrames;
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

//MOVED
int socket_for_client, client_connection;

unsigned char perm_double_zero[2] = { 0x00, 0x00 };
unsigned char perm_column_separator[2] = { 0x00, 0x0d };
unsigned char perm_column_value_separation[4] = { 0x00, 0x00, 0x00, 0x01 };
unsigned char perm_null_element[4] = { 0xff, 0xff, 0xff, 0xff };
//MOVED

//threads
pthread_t th_FrameClient;
pthread_t th_FrameData;
pthread_t th_Requests;
pthread_t th_PostgreSQL;
pthread_t th_Redirecting;
pthread_t th_INITSocket_Redirection;

PGconn* conn;
PGresult* res;

int server_count = 2;
server actual_server;
server neighbor_server_1;
server neighbor_server_2;
server server_to_redirect;
int port = 8042;
std::list<Requests> l_bufferRequestsForActualServer;    //CHANGED
std::list<Requests> l_bufferPreparedReq;
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
void* TraitementFrameData(void*);
void* TraitementRequests(void*);
void* SendPGSQL(void*);
void* INITSocket_Redirection(void*);
void* redirecting(void*);
void CQLtoSQL(SQLRequests);
void ConnexionPGSQL();
void exit_prog(int);
void send_to_server(int, string);
void server_identification();
void create_select_sql_query(string, string, vector<string>, char[2], string, int, int);    //CHANGED
void create_update_sql_query(string, string, vector<string>, char[2], int); //CHANGED
void create_insert_sql_query(string, string, vector<string>, vector<string>, char[2], int);    //CHANGED
void create_delete_sql_query(string, string, char[2], int); //CHANGED
int string_Hashing(string);
string get_ip_from_actual_server();
int connect_to_server(server, int);
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
#pragma endregion Prototypes

int main(int argc, char* argv[])
{
    if (argv[1] != NULL) {
        if (std::string(argv[1]) == "oui")
            bl_UseReplication = true;
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
    if (ConnPGSQLPrepStatements() == EXIT_FAILURE)
        exit_prog(EXIT_FAILURE);
    if (bl_UseReplication) {
        server_identification();
        int b = 0;
        while (b != 1)
        {
            cout << "tape 1 qd la redirection est lancee sur tous les serveurs" << endl;
            cin >> b;
            cout << endl;
        }
    }
    //Starting threads for sockets
    CheckThreadCreation += pthread_create(&th_FrameData, NULL, TraitementFrameData, NULL);
    CheckThreadCreation += pthread_create(&th_Requests, NULL, TraitementRequests, NULL);
    CheckThreadCreation += pthread_create(&th_PostgreSQL, NULL, SendPGSQL, NULL);
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
    sockDataClient = INITSocket(sockServer, argv[2]);
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
    while (1) {
        sockServer = 0;
        sockServer = recv(sockDataClient, buffData, sizeof(buffData), 0);
        if (sockServer > 0) {
            timestamp("Received frame", std::chrono::high_resolution_clock::now());
            l_bufferFrames.push_front(buffData);
            timestamp("Frame pushed", std::chrono::high_resolution_clock::now());
        }
    }
    logs("main() : Break input... Ending program");
    return EXIT_SUCCESS;
}

#pragma region Requests
void* TraitementFrameData(void* arg) {
    unsigned long int sommeSize = 0;
    bool bl_partialRequest = false;
    char partialRequest[300];
    char partialHeader[13];
    try {
        unsigned char test[10240];
        int autoIncrementRequest = 0;
        while (1) {
            if (l_bufferFrames.size() > 0) {
                timestamp("Detected Frame in buffer, starting reading", std::chrono::high_resolution_clock::now());
                memset(&test[0], 0, 10240);
                memset(&header[0], 0, 13);
                bl_lastRequestFrame = false;
                sommeSize = 0;
                if (bl_partialRequest){     //TODO check when partial request pop, can not control that
                    cout<<"AH"<<endl;
                    memcpy(test, &partialHeader[0], 13);
                    memcpy(&test[13], &partialRequest[13], 300);
                    for(int i=13; i<300; i++){
                        printf("0x%x ",test[i]);
                        if(test[i] == 0x00){
                            cout<<"ok?"<<endl;
                            memcpy(&test[i], l_bufferFrames.back(), 10240-i);
                            for(int j = 0;j<800; j++){printf("0x%x ",test[i]);}
                            i = 300;
                            bl_partialRequest = false;
                            cout<<"ok"<<endl;
                        }
                    }
                }
                else
                    memcpy(test, l_bufferFrames.back(), 10240);
                l_bufferFrames.pop_back();
                while (!bl_lastRequestFrame && !bl_partialRequest) {
                    autoIncrementRequest++;
                    memcpy(header, &test[sommeSize], 13);
                    s_Requests.size = (unsigned int)header[11] * 256 + (unsigned int)header[12];;
                    memcpy(s_Requests.request, &test[13 + sommeSize], s_Requests.size);
                    memcpy(s_Requests.opcode, &test[4 + sommeSize], 1);
                    memcpy(s_Requests.stream, &test[2 + sommeSize], 2);
                    s_Requests.origin = 0;
                    sommeSize += s_Requests.size + 13;    //Request size + header size(13) + 3 hex values at the end of the request
                    if (fichier) {
                        fichier << "Requete N° " << autoIncrementRequest << ", Taille : " << s_Requests.size << " : " << s_Requests.request << endl;
                    }
                    if (test[sommeSize-1] == 0x00)
                        bl_partialRequest = true;
                    if (test[sommeSize] == 0x00 && test[sommeSize + 1] == 0x01 && test[sommeSize + 2] == 0x00) {
                        sommeSize = sommeSize + 3;
                    }
                    if (test[sommeSize] == 0x00)
                        bl_lastRequestFrame = true; //Fin de la frame
                    else if (test[sommeSize - 2] == 0x04)
                        sommeSize = sommeSize - 2;
                    if (!bl_partialRequest){
                        if(s_Requests.opcode[0] == _QUERY_STATEMENT){
                            if (s_Requests.request[0] == 'U' && s_Requests.request[1] == 'S' && s_Requests.request[2] == 'E') 
                                l_bufferRequestsForActualServer.push_front(s_Requests);
                            else if (bl_UseReplication)
                                l_bufferRequests.push_front(s_Requests);
                            else
                                l_bufferRequestsForActualServer.push_front(s_Requests);
                        }
                        else if(s_Requests.opcode[0] == _PREPARE_STATEMENT){
                              //for(int te = 0; te<1024;te++){cout<<s_Requests.request[te];}     
                              PrepStatementResponse(header, s_Requests.request);
                            }
                            // l_bufferPreparedReq.push_front(s_Requests);
                        else if (s_Requests.opcode[0] == _OPTIONS_STATEMENT)
                            logs("DO THIS FUCKING ISALIVE REQUEST FRANZICHE", WARNING);
                        timestamp("Request pushed", std::chrono::high_resolution_clock::now());
                    }
                    else{
                        logs("Partial request", WARNING);
                        memcpy(partialRequest, &s_Requests.request[0], s_Requests.size);
                        memcpy(partialHeader, &header, 13);
                    }
                    memset(s_Requests.request, 0, s_Requests.size);
                }
                timestamp("Frame OK", std::chrono::high_resolution_clock::now());
            }
        }
    }
    catch (std::exception const& e) {
        logs("TraitementRequests() : " + std::string(e.what()), ERROR);
    }
    pthread_exit(NULL);
    pthread_exit(NULL);
}

void* TraitementRequests(void* arg) {
    SQLRequests tempReq;
    char TempReq[1024];
    try {
        while (1) {
            if (l_bufferRequestsForActualServer.size() > 0) {
                timestamp("Frame for actual server", std::chrono::high_resolution_clock::now());
                strcpy(TempReq, l_bufferRequestsForActualServer.back().request);
                tempReq.request = std::string(TempReq);
                memcpy(tempReq.stream, l_bufferRequestsForActualServer.back().stream, 2);
                //ADDED
                tempReq.origin = l_bufferRequestsForActualServer.back().origin;
                //ENDADDED
                l_bufferRequestsForActualServer.pop_back();
                timestamp("Calling CQLToSQL", std::chrono::high_resolution_clock::now());
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
    conninfo = "user = postgres";
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
    while (1)
    {
        if (l_bufferPGSQLRequests.size() > 0)
        {
            strcpy(requestPGSQL, l_bufferPGSQLRequests.back().request.c_str());
            memcpy(stream, l_bufferPGSQLRequests.back().stream, 2);
            strcpy(key_name, l_bufferPGSQLRequests.back().key_name);
            strcpy(key, l_bufferPGSQLRequests.back().key);
            pos_key = l_bufferPGSQLRequests.back().pos_key;
            //ADDED
            origin = l_bufferPGSQLRequests.back().origin;
            //ENDADDED

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

                //ADDED
                if (origin == 0)
                    write(sockDataClient, full_text, pos);
                else if (origin == 1)
                    write(client_connection, full_text, pos);
                //ENDADDED

                printf("\n");
            }

            else if (PQresultStatus(res) == PGRES_COMMAND_OK)
            {
                cout << "Command OK" << endl;
                unsigned char response_cmd_ok[13] = { 0x84, 0x00 , stream[0], stream[1], 0x08, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01 };
                //ADDED
                if (origin == 0)
                    write(sockDataClient, response_cmd_ok, 13);
                else if (origin == 1)
                    write(client_connection, response_cmd_ok, 13);
                //ENDADDED
                //TODO SEND TO CASSANDRA OK
            }

            else
            {
                cout << "Erreur dans la requête" << endl;
                cout << PQresultErrorMessage(res);
                //TODO SEND TO CASSANDRA NOK
            }
            //write(sockDataClient, &stream[0], 2);

            PQclear(res);
        }
    }
}

#pragma endregion PostgreSQL

#pragma region CQL_SQL
void CQLtoSQL(SQLRequests Request_incoming_cql_query)
{
    fields.clear();
    values.clear();
    columns.clear();
    _incoming_cql_query = Request_incoming_cql_query.request;
    //cout << _incoming_cql_query << endl;
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
            create_select_sql_query(table, key, fields, Request_incoming_cql_query.stream, key_name, pos_key, Request_incoming_cql_query.origin);     //CHANGED
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
            create_update_sql_query(table, key, values, Request_incoming_cql_query.stream, Request_incoming_cql_query.origin);    //CHANGED
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
            create_insert_sql_query(table, key, columns, values, Request_incoming_cql_query.stream, Request_incoming_cql_query.origin);   //CHANGED             //Appel de la fonction de conversion pour du RWCS
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
            create_delete_sql_query(table, key, Request_incoming_cql_query.stream, Request_incoming_cql_query.origin);  //CHANGED
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
        //ADDED
        if (Request_incoming_cql_query.origin == 0)
            write(sockDataClient, UseResponse, sizeof(UseResponse));
        else if (Request_incoming_cql_query.origin == 1)
            write(client_connection, UseResponse, sizeof(UseResponse));
        //ENDADDED
    }
    else {
        logs("CQLtoSQL() : Type de requete non reconnu. Requete : " + _incoming_cql_query, ERROR);      //TODO peut-être LOG ça dans un fichier de logs de requetes?
    }
    timestamp("CQLtoSQLDone", std::chrono::high_resolution_clock::now());
}

void create_select_sql_query(string _table, string _key, vector<string> _fields, char id[2], string _key_name, int pos_key, int origin) //CHANGED
{
    SQLRequests TempSQLReq;
    memcpy(TempSQLReq.stream, id, 2);
    strcpy(TempSQLReq.key, _key.c_str());
    strcpy(TempSQLReq.key_name, _key_name.c_str());
    TempSQLReq.pos_key = pos_key;
    //ADDED
    TempSQLReq.origin = origin;
    //ENDADDED
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

void create_update_sql_query(string _table, string _key, vector<string> _values, char id[2], int origin)    //CHANGED
{
    //Comme on update uniquement la colonne value d'une table, il faut utiliser la clause CASE sur la valeur de la colonne column_name
    //Dans _values, les éléments "impairs" (le premier, troisième,...) sont les colonnes à update et les "pairs" sont les valeurs de ces colonnes
    SQLRequests TempSQLReq;
    memcpy(TempSQLReq.stream, id, 2);
    //ADDED
    TempSQLReq.origin = origin;
    //ENDADDED
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

void create_insert_sql_query(string _table, string _key, vector<string> _columns, vector<string> _values, char id[2], int origin)  //CHANGED
{
    SQLRequests TempSQLReq;
    memcpy(TempSQLReq.stream, id, 2);
    //ADDED
    TempSQLReq.origin = origin;
    //ENDADDED
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

void create_delete_sql_query(string _table, string _key, char id[2], int origin)    //CHANGED
{
    SQLRequests TempSQLReq;
    memcpy(TempSQLReq.stream, id, 2);
    //ADDED
    TempSQLReq.origin = origin;
    //ENDADDED
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

#pragma endregion Utils

#pragma region Listening
void* INITSocket_Redirection(void* arg)
{
    struct sockaddr_in address;
    Requests req;
    char buffer[1024];

    socket_for_client = socket(AF_INET, SOCK_STREAM, 0);
    memset(&address, '0', sizeof(address));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    bind(socket_for_client, (struct sockaddr*)&address, sizeof(address));

    listen(socket_for_client, 10);

    client_connection = accept(socket_for_client, (struct sockaddr*)NULL, NULL);

    recv(client_connection, buffer, sizeof(buffer), 0);
    strcpy(req.request, buffer);
    //ADDED
    req.origin = 1;
    //ENDADDED
    l_bufferRequestsForActualServer.push_front(req);
    cout << endl << buffer << endl;

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

    cout << get_ip_from_actual_server() << endl;
    cout << "Serveur #" << actual_server.server_id << ", Nom : " << actual_server.server_name << ", Adresse IP: " << actual_server.server_ip_address << endl;

    //On crée les connexions permanentes avec les serveurs voisins
    socket_neighbor_1 = connect_to_server(neighbor_server_1, port);               //...
    //socket_neighbor_2 = connect_to_server(neighbor_server_2, port);
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
    logs("connect_to_server() : Connexion établie avec " + _server_to_connect.server_ip_address);
    // cout << endl << "Connexion etablie avec " << ip_address << endl;

    return sock_to_server;
}

void send_to_server(int _socketServer, string _query_to_send)
{
    const char* cql_query = _query_to_send.c_str();

    send(_socketServer, cql_query, strlen(cql_query), 0);
    cout << endl << "Incoming query sent" << endl;
}
#pragma endregion Server_connection

#pragma region Redirecting
void* redirecting(void* arg)
{
    Requests req;
    string tempReq;
    // char stream[2];
    while (1)
    {
        //On hash la clé extraite de la requête via la fonction string_Hashing()
        if (l_bufferRequests.size() > 0)
        {
            tempReq = l_bufferRequests.back().request;
            strcpy(req.stream, l_bufferRequests.back().stream);
            strcpy(req.opcode, l_bufferRequests.back().opcode);
            // req.RequestNumber=stream;
            // req.RequestOpcode='0x07';
            l_bufferRequests.pop_back();
            cout << "redirecting() pop back ok" << endl;
            string key_from_cql_query = key_extractor(tempReq);

            int hashed_key = string_Hashing(key_from_cql_query);
            cout << hashed_key << endl;

            //On détermine le serveur vers lequel rediriger
            int range_id = hashed_key % server_count;
            cout << range_id << endl;

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
                    cout << "Requete a rediriger vers le voisin " << server_to_redirect.server_name << endl;
                    send_to_server(socket_neighbor_1, tempReq);
                }
                //Si non on crée la connection et on envoie
                else
                {
                    cout << "Requete a rediriger vers " << server_to_redirect.server_name << endl;
                    send_to_server(connect_to_server(server_to_redirect, port), tempReq);
                }
            }
            else
            {
                //Envoi vers PostgreSQL
                // req.Request=tempReq.c_str();
                strcpy(req.request, tempReq.c_str());
                l_bufferRequestsForActualServer.push_front(req);
                cout << "Requete a envoyer vers PostgreSQL" << endl;
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
#pragma endregion Redirecting