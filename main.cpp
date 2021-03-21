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
#include "c_Socket.h"
#include "c_Logs.h"
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

typedef int SOCKET;

#pragma region DeleteForProd
string const nomFichier("/home/tfe_rwcs/Couche_Logicielle/Request.log");
// string const LogFile("Logs");
ofstream fichier(nomFichier.c_str());
time_t t = time(0);
std::chrono::steady_clock::time_point startTimestamp;
std::chrono::steady_clock::time_point endTimestamp;
c_Logs o_Logs;
#pragma endregion DeleteForProd

#pragma region Structures

struct Requests{
    char opcode[1];
    char stream[2];
    unsigned long int size;
    char request[1024];
};
struct SQLRequests{
    char stream[2];
    string request;
};
struct sockaddr_un s_PGSQL_addr;

struct server
{
    string server_name;
    int server_id;
    string server_ip_address;
};

#pragma endregion Structures

#pragma region Global
Requests s_Requests;
char buffData[1024];
char CQLResponse[1024];
char header[12];
std::list<char*> l_bufferFrames;
std::list<Requests> l_bufferRequests;
std::list<SQLRequests> l_bufferPGSQLRequests;
std::string incoming_cql_query, translated_sql_query;
size_t from_sub_pos, where_sub_pos, limit_sub_pos, set_sub_pos, values_sub_pos = 0;
std::string select_clause, from_clause, where_clause, update_clause, set_clause, insert_into_clause, values_clause, delete_clause = "";
std::string table, key = "";
vector<std::string> fields, values, columns;
std::string returned_select_sql_query = "";
std::string LowerRequest = "";
std::string _incoming_cql_query = "";

bool bl_UseReplication = false;
bool bl_lastRequestFrame = false;
SOCKET sockPGSQL;

pthread_t th_FrameClient;
pthread_t th_FrameData;
pthread_t th_Requests;
pthread_t th_PostgreSQL;

const char *conninfo;
PGconn *conn;
PGresult *res;

//Variables
//int server_count = 6;
int server_count = 2;
server actual_server;
server neighbor_server_1;
server neighbor_server_2;
server server_to_redirect;
int port = 8042;
std::list<Requests> l_bufferRequestsForActualServer, l_bufferRequestsFromServer;
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
void *TraitementFrameData(void*);
void *TraitementRequests(void*);
void ConnexionPGSQL();
void *SendPGSQL(void*);
void exit_prog(int CodeEXIT);
// void logs(std::string msg, LogStatus LogStatusText = LogStatus::INFO);
void create_select_sql_query(string _table, string _key, vector<string> _fields, char id[2]);
void create_update_sql_query(string _table, string _key, vector<string> _values, char id[2]);
void create_insert_sql_query(string _table, string _key, vector<string> _columns,  vector<string> _values, char id[2]);
void create_delete_sql_query(string _table, string _key, char id[2]);
vector<string> extract_select_data(string _select_clause);
string extract_from_data(string _form_clause);
string extract_where_data(string _where_clause);
string extract_update_data(string _update_clause);
vector<string> extract_set_data(string _set_clause);
string extract_insert_into_data_table(string _insert_into_clause);
vector<string> extract_insert_into_data_columns(string _insert_into_clause);
vector<string> extract_values_data(string _values_clause);
string extract_delete_data(string _delete_clause);
void CQLtoSQL(SQLRequests Request_incoming_cql_query);
int string_Hashing(string _key_from_cql_query);
// string get_ip_from_actual_server();
int connect_to_server(server _server_to_connect, int _port_to_connect);
void send_to_server(int _socketServer, string _query_to_send);
void* INITSocket_Redirection(void* arg);
void* redirecting(void* arg);
void server_identification();
string key_extractor(string _incoming_cql_query);
pthread_t th_Redirecting;
pthread_t th_INITSocket_Redirection;
#pragma endregion Prototypes

int main(int argc, char *argv[])
{  
    c_Logs o_Logs;
    startTimestamp = std::chrono::steady_clock::now();
    if (argv[1] != NULL){
      if(std::string(argv[1]) == "oui")
        bl_UseReplication = true;
    }
    int CheckThreadCreation = 0;    
    if (bl_UseReplication){
        o_Logs.logs("Starting Proxy...Replication mode selected");
        CheckThreadCreation += pthread_create(&th_INITSocket_Redirection, NULL, INITSocket_Redirection, NULL);
        if(CheckThreadCreation !=0)
            o_Logs.logs("main() : Thread th_INITSocket_Redirection creation failed", o_Logs.ERROR);
        else
            o_Logs.logs("main() : Thread th_INITSocket_Redirection created");
        int a = 0;
        while (a != 1)
        {
            cout << "tape 1 qd la couche logicielle est lancee sur tous les serveurs" << endl;
            cin >> a;
            cout << endl;
        }
    }
    else
        o_Logs.logs("Starting Proxy...Standalone mode selected");
    ConnexionPGSQL();   //Connexion PGSQL
    
    if (bl_UseReplication){
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
    if (bl_UseReplication){
      CheckThreadCreation += pthread_create(&th_Redirecting, NULL, redirecting, NULL);
    }
    //Check if threads have been created
    if (CheckThreadCreation !=0){
        o_Logs.logs("main() : Error while creating threads", o_Logs.ERROR);
        exit_prog(EXIT_FAILURE);
    }
    
    //Réception des frames en continu et mise en buffer
    INITSocket();   //TODO move to c++ function add add logs
    CheckThreadCreation += pthread_create(&th_FrameClient, NULL, TraitementFrameClient, NULL);
    
    if (CheckThreadCreation !=0){
        o_Logs.logs("main() : Error while creating threads", o_Logs.ERROR);
        exit_prog(EXIT_FAILURE);
    }
    o_Logs.logs("main() : Starting Done");
    while(1){
        sockServer = 0;
        sockServer = recv(sockDataClient, buffData, sizeof(buffData),0);
        if(sockServer > 0){
            l_bufferFrames.push_front(buffData);
        }
    }
    o_Logs.logs("main() : Break input... Ending program");
    return EXIT_SUCCESS;
}

#pragma region Requests
void* TraitementFrameData(void *arg){
    unsigned long int sommeSize = 0;
    try{
        while(1){
            if(l_bufferFrames.size()>0){
                bl_lastRequestFrame = false;
                sommeSize = 0;
                while(bl_lastRequestFrame == false){
                    memcpy(header,l_bufferFrames.back()+sommeSize,13);	    
                    s_Requests.size = (unsigned int)header[11+sommeSize] * 256 + (unsigned int)header[12+sommeSize];
                    if((unsigned int)header[10+sommeSize]>0){
                        o_Logs.logs("TraitementFrameData() : Oupsi, frame un peu longue, adapter le code si cette erreur apparait", o_Logs.ERROR);
                        exit (EXIT_FAILURE);
                    }
                    memcpy(s_Requests.request, l_bufferFrames.back()+13, s_Requests.size);
                    memcpy(s_Requests.opcode, header+4+sommeSize,1);
                    memcpy(s_Requests.stream, header+2+sommeSize,2);
                    sommeSize += s_Requests.size+16;    //Request size + header size(13) + 3 hex values at the end of the request
                    if (sizeof(l_bufferFrames.back()) <= sommeSize){
                        bl_lastRequestFrame = true;
                    }
                    if (fichier){
                        fichier<<"------------Decoupage Requete------------"<<endl;
                        fichier<<"Stream : "<<s_Requests.opcode<<endl;
                        fichier<<"Opcode : "<<s_Requests.opcode<<endl;
                        fichier<<"Taille Requete : "<<s_Requests.size<<endl;
                        fichier<<"Requete : "<<s_Requests.request<<endl;
                        fichier<<"-------------------END-------------------"<<endl;
                    }
                    if (bl_UseReplication)
                    l_bufferRequests.push_front(s_Requests);
                    else
                    l_bufferRequestsForActualServer.push_front(s_Requests);
                    memset(s_Requests.request,0,s_Requests.size);
                    l_bufferFrames.pop_back();
                }
            }
        }
    }
    catch(std::exception const& e){
        o_Logs.logs("TraitementRequests() : "+std::string(e.what()), o_Logs.ERROR);
    } 
    pthread_exit(NULL);
    pthread_exit(NULL);
}

void* TraitementRequests(void *arg){
    SQLRequests tempReq;
    char TempReq[1024];
    try{
        while(1){
            if(l_bufferRequestsForActualServer.size()>0){
                strcpy(TempReq,l_bufferRequestsForActualServer.back().request);
                tempReq.request = std::string(TempReq);
                memcpy(tempReq.stream, l_bufferRequestsForActualServer.back().stream, 2);
                l_bufferRequestsForActualServer.pop_back(); 
                CQLtoSQL(tempReq);
            }
        }
    }
    catch(std::exception const& e){
        o_Logs.logs("TraitementRequests() : " + std::string(e.what()), o_Logs.ERROR);
    } 
    pthread_exit(NULL);
}

#pragma endregion Requests

#pragma region PostgreSQL
void ConnexionPGSQL(){
    conninfo = "user = postgres";
    conn = PQconnectdb(conninfo);
    /* Check to see that the backend connection was successfully made */
    if (PQstatus(conn) != CONNECTION_OK)
    {
        o_Logs.logs("ConnexionPGSQL() : Connexion to database failed", o_Logs.ERROR);
        exit_prog(EXIT_FAILURE);
    }
    /* Set always-secure search path, so malicious users can't take control. */
    res = PQexec(conn, "SELECT pg_catalog.set_config('search_path', 'public', false)");
    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        PQclear(res);   //TODO check what is it?
        o_Logs.logs("ConnexionPGSQL() : Secure search path error", o_Logs.ERROR);
        exit_prog(EXIT_FAILURE); 
    }
    else
	    o_Logs.logs("ConnexionPGSQL() : Connexion to PostgreSQL sucess");
    PQclear(res);
}

void *SendPGSQL(void *arg){     //TODO logs not done yet, waiting for "prod" code
    PGresult *res;
    char requestPGSQL[1024];
    char stream[2];
    while(1){
        if(l_bufferPGSQLRequests.size() > 0){
            strcpy(requestPGSQL, l_bufferPGSQLRequests.back().request.c_str());
            memcpy(stream, l_bufferPGSQLRequests.back().stream,2);
            // cout<<"SendPGSQL() COPY ok"<<endl;
            l_bufferPGSQLRequests.pop_back();
            // cout<<requestPGSQL<<endl;
            // cout<<"Opcode : "<<opCode<<endl;
            // cout<<"Stream : "<<std::hex<<stream[0]<<stream[1]<<endl;
            res = PQexec(conn, requestPGSQL);
    	      if (PQresultStatus(res) == PGRES_TUPLES_OK){
                //Affichage des En-têtes
                int nFields = PQnfields(res);
          	    for (int i = 0; i < nFields; i++)
          		      printf("%-15s", PQfname(res, i));
         	    printf("\n\n");
                //Affichage des résultats
          	    for (int i = 0; i < PQntuples(res); i++)
          	    {
                    for (int j = 0; j < nFields; j++)
                        printf("%-15s", PQgetvalue(res, i, j));
                    printf("\n");
    	        }
                //Pour tester, à optimiser              
	    }
            else if(PQresultStatus(res) == PGRES_COMMAND_OK){
                cout<<"Command OK"<<endl;
                //TODO SEND TO CASSANDRA OK
            }
            else{
                cout<<"Erreur dans la requête"<<endl;
                cout<<PQresultErrorMessage(res);
                //TODO SEND TO CASSANDRA NOK
            }
            //write(sockDataClient, &stream[0], 2);
            write(sockDataClient, stream, 2);
            PQclear(res);
        }
    }
}

#pragma endregion PostgreSQL

#pragma region CQL_SQL
void create_select_sql_query(string _table, string _key, vector<string> _fields, char id[2])
{
    SQLRequests TempSQLReq;
    memcpy(TempSQLReq.stream, id,2);
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
    else{
        TempSQLReq.request += ";";
    }
    l_bufferPGSQLRequests.push_front(TempSQLReq);
    return ;
}

void create_update_sql_query(string _table, string _key, vector<string> _values, char id[2] )
{
    //Comme on update uniquement la colonne value d'une table, il faut utiliser la clause CASE sur la valeur de la colonne column_name
    //Dans _values, les éléments "impairs" (le premier, troisième,...) sont les colonnes à update et les "pairs" sont les valeurs de ces colonnes
    SQLRequests TempSQLReq;
    strcpy(TempSQLReq.stream, id);
    TempSQLReq.request = "UPDATE " + _key + " SET value = ";
    //On regarde si on a plusieurs champs à update
    if (_values.size() > 2)
    {
        TempSQLReq.request += "(CASE ";
        for (unsigned int i = 0; i < _values.size(); i = i + 2)
        {
            //On CASE sur chaque colonne CQL, càd chaque valeur que peut prendre la colonne column_name
            TempSQLReq.request += "when column_name = '" + _values[i] + "' then '" + _values[i+1] + "' ";
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
    // return returned_update_sql_query;
}

void create_insert_sql_query(string _table, string _key, vector<string> _columns,  vector<string> _values, char id[2])
{
    SQLRequests TempSQLReq;
    strcpy(TempSQLReq.stream, id);
    TempSQLReq.request = "CREATE TABLE " + _key + " (column_name varchar(255), value varchar(255)); ";
    cout<<TempSQLReq.request<<endl;
    l_bufferPGSQLRequests.push_front(TempSQLReq);
    for (unsigned int i = 1; i < _values.size(); i++)
    {
        TempSQLReq.request = "INSERT INTO " + _key + " (column_name, value) VALUES('" + _columns[i] + "', '" + _values[i] + "'); ";
        cout<<TempSQLReq.request<<endl;
        l_bufferPGSQLRequests.push_front(TempSQLReq);
    }
    //TODO
    //return returned_insert_sql_query;
}

void create_delete_sql_query(string _table, string _key, char id[2])
{
    SQLRequests TempSQLReq;
    strcpy(TempSQLReq.stream, id);
    TempSQLReq.request = "DROP TABLE " + _key + " ;";
    l_bufferPGSQLRequests.push_front(TempSQLReq);
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
        // token.erase(remove(token.begin(), token.end(), ' '), token.end());
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
    // _select_clause.erase(remove(_select_clause.begin(), _select_clause.end(), ' '), _select_clause.end());
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

string extract_where_data(string where_clause_data)
{
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

void CQLtoSQL(SQLRequests Request_incoming_cql_query)
{
    fields.clear();
    values.clear();
    columns.clear();
    _incoming_cql_query = Request_incoming_cql_query.request;
    LowerRequest = _incoming_cql_query;
    std::transform(LowerRequest.begin(), LowerRequest.end(), LowerRequest.begin(), [](unsigned char c){ return std::tolower(c); });
    if (LowerRequest.substr(0, 6) == "select")
    {
        try{
            where_sub_pos = limit_sub_pos = sizeof(LowerRequest)-1;
            cout << "Type de la requete : SELECT" << endl;
            if(LowerRequest.find("from ") == _NPOS)
                throw string("Erreur : Pas de FROM dans la requete");
            else{
                from_sub_pos = LowerRequest.find("from ");
                select_clause = _incoming_cql_query.substr(6, from_sub_pos - 6);
                fields = extract_select_data(select_clause);
            }
            if(LowerRequest.find("where ") == _NPOS){
                throw string("Erreur : Pas de WHERE dans la requete");  //TODO gérer les cas sans WHERE?
            }
            else{
                where_sub_pos = LowerRequest.find("where ");
                from_clause = _incoming_cql_query.substr(from_sub_pos, where_sub_pos - from_sub_pos);
                table = extract_from_data(from_clause);
            }
            if(LowerRequest.find("limit ") != _NPOS){
                limit_sub_pos = LowerRequest.find("limit ");
                where_clause = _incoming_cql_query.substr(where_sub_pos, limit_sub_pos - where_sub_pos);
                key = extract_where_data(where_clause);
            }
            //IF WHERE && !LIMIT
            else if(where_sub_pos +1 < LowerRequest.length()){
                where_clause = _incoming_cql_query.substr(where_sub_pos, _NPOS);
                key = extract_where_data(where_clause);
            }
            //Affichage des paramètres
            cout << "Table: " << table;
            if(key != "")
                cout<<" - Key: " << key << " Fields : ";
            for (string field : fields)
            {
                cout << field << " __ ";
            }
            cout << endl;
            //Appel de la fonction de conversion pour du RWCS
            create_select_sql_query(table, key, fields, Request_incoming_cql_query.stream);
        }
         catch(std::string const& chaine)
        {
            cerr << chaine << endl;
            o_Logs.logs(chaine);
        }
        catch(std::exception const& e)
        {
            cerr << "ERREUR : " << e.what() << endl;
            o_Logs.logs(e.what());
        }
    }
    else if (LowerRequest.substr(0, 6) == "update")
    {
        try{
            cout << "Type de la requete : UPDATE" << endl;
            if(LowerRequest.find("set ") == _NPOS ||LowerRequest.find("where ") == _NPOS)   //If missing clause
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
                cout << values[i] << " => " << values[i+1] << " __ ";
            }
            cout << endl;

            //Call conversion function          
            create_update_sql_query(table, key, values, Request_incoming_cql_query.stream);
            // l_bufferPGSQLRequests.push_front(Request_incoming_cql_query);
        }
        catch(std::string const& chaine)
        {
            cerr << chaine << endl;
            o_Logs.logs(chaine);
        }
        catch(std::exception const& e)
        {
            cerr << "ERREUR : " << e.what() << endl;
            o_Logs.logs(e.what());
        }
    }
    else if (LowerRequest.substr(0, 11) == "insert into")
    {
        try{
            cout << "Type de la requete : INSERT" << endl;
            if(LowerRequest.find("values ") == _NPOS)   //If missing clause
                    throw string("Erreur : Missing Clause VALUES in INSERT INTO clause");
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
            create_insert_sql_query(table, key, columns, values, Request_incoming_cql_query.stream);             //Appel de la fonction de conversion pour du RWCS
        }
        catch(std::string const& chaine)
        {
            cerr << chaine << endl;
            o_Logs.logs(chaine);
        }
        catch(std::exception const& e)
        {
            cerr << "ERREUR : " << e.what() << endl;
            o_Logs.logs(e.what());
        }
    }
    else if (LowerRequest.substr(0, 6) == "delete")
    {
        try{
            cout << "Type de la requete : DELETE" << endl;
            if(LowerRequest.find("where ") == _NPOS)   //If missing clause
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
            create_delete_sql_query(table, key, Request_incoming_cql_query.stream);
        }
        catch(std::string const& chaine)
        {
            cerr << chaine << endl;
            o_Logs.logs(chaine);
        }
        catch(std::exception const& e)
        {
            cerr << "ERREUR : " << e.what() << endl;
            o_Logs.logs(e.what());
        }
    }
    else{
        o_Logs.logs("CQLtoSQL() : Type de requete non reconnu. Requete : " + _incoming_cql_query);
    }
}
#pragma endregion CQL_SQL

#pragma region Utils

// void logs(string msg, LogStatus LogStatusText)
// {
//     std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();    //Get time clock system
//     time_t tt =  std::chrono::system_clock::to_time_t(timestamp);
//     ofstream file("Logs", std::ios_base::app);
//     if (file){
//         file<<"Time : "<<ctime(&tt)<<" : "<<LogStatusText<<" : "<<msg<<endl;
//     }
//     file.close();
// }

void exit_prog(int codeEXIT){

    exit(codeEXIT);
}

#pragma endregion Utils

#pragma region Listening
void* INITSocket_Redirection(void* arg)
{
    int socket_for_client, client_connection;
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
    l_bufferRequestsForActualServer.push_front(req);
    cout << endl << buffer << endl;

    return NULL;
}
#pragma endregion Listening

#pragma region Preparation
void server_identification()
{
    char *s;
    s = inet_ntoa(serv_addr.sin_addr);
    string ipAddr = std::string(s);
    o_Logs.logs("serveur_identification() : IP address : " + ipAddr);
    if (ipAddr == server_A.server_ip_address)
    {
        actual_server = server_A;
        neighbor_server_1 = server_B;
        //neighbor_server_2 = server_F;
    }
    else if (ipAddr == server_B.server_ip_address)
    {
        actual_server = server_B;
        neighbor_server_1 = server_A;
        //neighbor_server_1 = server_C;
        //neighbor_server_2 = server_A;
    }
    else if (ipAddr == server_C.server_ip_address)
    {
        actual_server = server_C;
        neighbor_server_1 = server_D;
        neighbor_server_2 = server_B;
    }
    else if (ipAddr == server_D.server_ip_address)
    {
        actual_server = server_D;
        neighbor_server_1 = server_E;
        neighbor_server_2 = server_C;
    }
    else if (ipAddr == server_E.server_ip_address)
    {
        actual_server = server_E;
        neighbor_server_1 = server_F;
        neighbor_server_2 = server_D;
    }
    else if (ipAddr == server_F.server_ip_address)
    {
        actual_server = server_F;
        neighbor_server_1 = server_A;
        neighbor_server_2 = server_E;
    }

    // cout << "Serveur #" << actual_server.server_id << ", Nom : " << actual_server.server_name << ", Adresse IP: " << actual_server.server_ip_address << endl;
    // o_Logs.logs("Serveur #"+actual_server.server_id +", Nom : "+actual_server.server_name+", Adresse IP: "+actual_server.server_ip_address); //TODO add this but bug for the int to log
    //On crée les connexions permanentes avec les serveurs voisins
    socket_neighbor_1 = connect_to_server(neighbor_server_1, port);               //...
    //socket_neighbor_2 = connect_to_server(neighbor_server_2, port);
}
/*
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
*/
#pragma endregion Preparation

#pragma region Server_connection
int connect_to_server(server _server_to_connect, int _port_to_connect)
{
    int sock_to_server = 0;     //TODO delete multiple declaration, not in priority
    struct sockaddr_in serv_addr;
    const char* ip_address = _server_to_connect.server_ip_address.c_str();

    sock_to_server = socket(AF_INET, SOCK_STREAM, 0);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(_port_to_connect);

    inet_pton(AF_INET, ip_address, &serv_addr.sin_addr);
    connect(sock_to_server, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

    cout << endl << "Connexion etablie avec " << ip_address << endl;

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
    while (1)
    {
        //On hash la clé extraite de la requête via la fonction string_Hashing()
        if (l_bufferRequests.size() > 0)
        {
            tempReq = l_bufferRequests.back().request;
            strcpy(req.stream, l_bufferRequests.back().stream);
            strcpy(req.opcode, l_bufferRequests.back().opcode);

            l_bufferRequests.pop_back();
            cout<<"redirecting() pop back ok"<<endl;
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
        cout << "Type de la requete : SELECT" << endl;
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
        cout << "Type de la requete : UPDATE" << endl;
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
        cout << "Type de la requete : INSERT" << endl;
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
        cout << "Type de la requete : DELETE" << endl;
        where_sub_pos = _incoming_cql_query.find(delete_clauses[1]);

        where_clause = _incoming_cql_query.substr(where_sub_pos, std::string::npos);

        //On extrait, affiche et retourne la clé
        key = extract_where_data(where_clause);
        cout << "Extracted Key: " << key << endl;

        return key;
    }
    else {
        o_Logs.logs("");
        exit(EXIT_FAILURE);
    }
}
#pragma endregion Redirecting
