#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
// #include <thread>
#include <pthread.h>
#include <list>
#include <vector>
#include <string.h>
#include <algorithm>
#include "c_Socket.h"
#include "libpq-fe.h"
#include <future>

//Debug libraries
#include <iostream>
#include <time.h>
#include <chrono>
#include <fstream>

#include <functional>       //...
#include <ifaddrs.h>

using namespace std;
using std::string;

typedef int SOCKET;

#pragma region DeleteForProd
string const nomFichier("/home/tfe_rwcs/Couche_Logicielle/Request.log");
ofstream fichier(nomFichier.c_str());
char date[9];
time_t t = time(0);
struct tm *timestamp;
void logs(const char *msg);
#pragma endregion DeleteForProd

#pragma region Structures
struct ThreadsInfos{

};
struct Requests{
    char opcode[1];
    char stream[2];
    unsigned long int size;
    char request[1024];
};
struct sockaddr_un s_PGSQL_addr;

#pragma endregion Structures

#pragma region Global
Requests s_Requests;
char buffData[1024];
char header[12];
std::list<char*> l_bufferFrames;
std::list<Requests> l_bufferRequests;
std::list<std::string> l_bufferPGSQLRequests;
std::string incoming_cql_query;
std::string translated_sql_query;
bool bl_lastRequestFrame = false;
SOCKET sockPGSQL;

pthread_t th_INITSocket;
pthread_t th_FrameClient;
pthread_t th_FrameData;
pthread_t th_Requests;
pthread_t th_PostgreSQL;

const char *conninfo;
PGconn *conn;
PGresult *res;
// PGnotify   *notify;
// int         nnotifies;

#pragma endregion Global

struct server                   //...
{
    string server_name;
    int server_id;
    string server_ip_address;
};

//Variables
//int server_count = 6;
int server_count = 2;
server actual_server;
server neighbor_server_1;
server neighbor_server_2;
server server_to_redirect;
int port = 8042;
std::list<std::string> l_bufferRequestsForActualServer, l_bufferRequestsFromServer;
int socket_neighbor_1, socket_neighbor_2;

pthread_t th_Redirecting;
pthread_t th_Waitng_for_client;

//Liste des serveurs
server server_A = { "RWCS-vServer1", 0, "192.168.82.55" };
server server_B = { "RWCS-vServer2", 1, "192.168.82.61" };
server server_C = { "RWCS-vServer3", 2, "192.168.82.63" };
server server_D = { "RWCS-vServer4", 3, "192.168.82.56" };
server server_E = { "RWCS-vServer5", 4, "192.168.82.58" };
server server_F = { "RWCS-vServer6", 5, "192.168.82.59" };

#pragma region Prototypes
void *TraitementFrameData(void*);
void *TraitementRequests(void*);
void ConnexionPGSQL();
void *SendPGSQL(void*);
void exit_prog(int CodeEXIT);
void logs(const char *msg);
// void printTimestamp(string text, std::chrono::system_clock clock);
string create_select_sql_query(string _table, string _key, vector<string> _fields);
string create_update_sql_query(string _table, string _key, vector<string> _values);
string create_insert_sql_query(string _table, string _key, vector<string> _columns,  vector<string> _values);
string create_delete_sql_query(string _table, string _key);
vector<string> extract_select_data(string _select_clause);
string extract_from_data(string _form_clause);
string extract_where_data(string _where_clause);
string extract_update_data(string _update_clause);
vector<string> extract_set_data(string _set_clause);
string extract_insert_into_data_table(string _insert_into_clause);
vector<string> extract_insert_into_data_columns(string _insert_into_clause);
vector<string> extract_values_data(string _values_clause);
string extract_delete_data(string _delete_clause);
string CQLtoSQL(string _incoming_cql_query);
#pragma endregion Prototypes

int string_Hashing(string _key_from_cql_query);          //...
string get_ip_from_actual_server();
int connect_to_server(server _server_to_connect, int _port_to_connect);
void send_to_server(int _socketServer, string _query_to_send);
void* INITSocket_Redirection(void* arg);
void* redirecting(void* arg);
void server_identification();
string key_extractor(string _incoming_cql_query);

int main(int argc, char *argv[])
{
    int CheckThreadCreation = 0;

    //Threaads for listening
    CheckThreadCreation += pthread_create(&th_INITSocket, NULL, INITSocket, NULL);
    cout << "INITSocket" << endl;
    CheckThreadCreation += pthread_create(&th_Waitng_for_client, NULL, INITSocket_Redirection, NULL);
    cout << "INITSocket_Redirection" << endl;

    int a = 0;
    while (a != 1)
    {
        cout << "tape 1 qd la couche logicielle est lancee sur tous les serveurs" << endl;
        cin >> a;
        cout << endl;
    }

    //Preparation
    ConnexionPGSQL();
    cout << "ConnexionPGSQL" << endl;
    server_identification();
    cout << "server_identification" << endl;

    //Threads for processing
    CheckThreadCreation += pthread_create(&th_FrameClient, NULL, TraitementFrameClient, NULL);
    CheckThreadCreation += pthread_create(&th_FrameData, NULL, TraitementFrameData, NULL);
    CheckThreadCreation += pthread_create(&th_Requests, NULL, TraitementRequests, NULL);
    CheckThreadCreation += pthread_create(&th_PostgreSQL, NULL, SendPGSQL, NULL);
    CheckThreadCreation += pthread_create(&th_Redirecting, NULL, redirecting, NULL);

    //Check if threads have been created
    if (CheckThreadCreation !=0){
        logs("main() : Error while creating threads, abording");
        exit_prog(EXIT_FAILURE);
    }

    int b = 0;
    while (b != 1)
    {
        cout << "tape 1 qd la redirection est lancee sur tous les serveurs" << endl;
        cin >> b;
        cout << endl;
    }

    cout << "OK" << endl;

    //l_bufferRequestsFromServer.push_front("SELECT level, psnid FROM azerty WHERE id = 'dallas' LIMIT 1;");

    //Réception des frames en continu et mise en buffer
    while(1){
        sockServer = 0;
        sockServer = recv(sockDataClient, buffData, sizeof(buffData),0);
        if(sockServer > 0){
            l_bufferFrames.push_front(buffData);
        }
    }

    return EXIT_SUCCESS;
}



#pragma region Listening
void* INITSocket_Redirection(void* arg)
{
    int socket_for_client, client_connection;
    struct sockaddr_in address;

    char buffer[1024];

    socket_for_client = socket(AF_INET, SOCK_STREAM, 0);
    memset(&address, '0', sizeof(address));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    bind(socket_for_client, (struct sockaddr*)&address, sizeof(address));

    listen(socket_for_client, 10);

    client_connection = accept(socket_for_client, (struct sockaddr*)NULL,NULL);

    recv(client_connection, buffer, sizeof(buffer), 0);
    l_bufferRequestsFromServer.push_front(buffer);
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



#pragma region PostgreSQL
void ConnexionPGSQL(){

    conninfo = "user = postgres";
    conn = PQconnectdb(conninfo);
    /* Check to see that the backend connection was successfully made */
    if (PQstatus(conn) != CONNECTION_OK)
    {
        fprintf(stderr, "Connection to database failed: %s", PQerrorMessage(conn));
        logs("ConnexionPGSQL() : Connexion to database failed");
        exit_prog(EXIT_FAILURE);
    }

    /* Set always-secure search path, so malicious users can't take control. */
    res = PQexec(conn, "SELECT pg_catalog.set_config('search_path', '', false)");
    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        fprintf(stderr, "SET failed: %s", PQerrorMessage(conn));
        PQclear(res);   //TODO check what is it?
        logs("ConnexionPGSQL() : Secure search path error");
        exit_prog(EXIT_FAILURE); 
    }
}

void *SendPGSQL(void *arg){
    PGresult *res;
    while(1){
        if(l_bufferPGSQLRequests.size() > 0){
            cout<<"Request SQL : "<<l_bufferPGSQLRequests.back()<<endl;
            res = PQexec(conn, l_bufferPGSQLRequests.back().data());
            std::cout<<"Response : "<<res<<endl;
            l_bufferPGSQLRequests.pop_back();
        }
    }
}
#pragma endregion PostgreSQL



#pragma region Requests
void* TraitementFrameData(void *arg){
    unsigned long int sommeSize = 0;
    while(1){
        if(l_bufferFrames.size()>0){
            bl_lastRequestFrame = false;
	        sommeSize = 0;
            while(bl_lastRequestFrame == false){
                memcpy(header,l_bufferFrames.back()+sommeSize,13);	    
                s_Requests.size = (unsigned int)header[11+sommeSize] * 256 + (unsigned int)header[12+sommeSize];
                if((unsigned int)header[10+sommeSize]>0){
                    printf("STOOOOOOPP IT");
                    exit (EXIT_FAILURE);    //TODO What the hell is this?
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
                l_bufferRequests.push_front(s_Requests);
                memset(s_Requests.request,0,s_Requests.size);
                l_bufferFrames.pop_back();
            }
        }
    }
    pthread_exit(NULL);
}

void* TraitementRequests(void* arg) {
    while (1) {
        if (l_bufferRequestsForActualServer.size() > 0) {
            translated_sql_query = CQLtoSQL(l_bufferRequestsForActualServer.back());
            l_bufferRequestsForActualServer.pop_back();
            l_bufferPGSQLRequests.push_front(translated_sql_query);
        }
    }
    pthread_exit(NULL);
}
#pragma endregion Requests



#pragma region CQL_SQL
string create_select_sql_query(string _table, string _key, vector<string> _fields)
{
    //Pour une requête SELECT, on va prendre toutes les données des deux colonnes de la table où le column_name correspond au champ voulu
    //On considère que le nom de la table est juste la key CQL, mais on pourrait imaginer une concaténation de la table CQL et de la key CQL -> le paramètre _table n'est pas utilisé ici
    //Pour la mise en forme de la réponse au format CQL, on va faire ça au retour du RWCS, siinon la requête d'envoi (ici) aurait été trop complexe

    string returned_select_sql_query = "SELECT * FROM " + _key;
    //On regarde si, dans la requête CQL, on voulait prendre * ou des champs particuliers
    if (_fields[0] != "*")
    {
        returned_select_sql_query += " WHERE column_name IN ('";
        for (string field : _fields)
        {
            returned_select_sql_query += field + "', '";
        }
        returned_select_sql_query = returned_select_sql_query.substr(0, returned_select_sql_query.length() - 3);
        returned_select_sql_query += ");";
    }
    return returned_select_sql_query;
}

string create_update_sql_query(string _table, string _key, vector<string> _values)
{
    //Comme on update uniquement la colonne value d'une table, il faut utiliser la clause CASE sur la valeur de la colonne column_name
    //Dans _values, les éléments "impairs" (le premier, troisième,...) sont les colonnes à update et les "pairs" sont les valeurs de ces colonnes
    string returned_update_sql_query = "UPDATE " + _key + " SET value = ";
    //On regarde si on a plusieurs champs à update
    if (_values.size() > 2)
    {
        returned_update_sql_query += "(CASE ";
        for (unsigned int i = 0; i < _values.size(); i = i + 2)
        {
            //On CASE sur chaque colonne CQL, càd chaque valeur que peut prendre la colonne column_name
            returned_update_sql_query += "when column_name = '" + _values[i] + "' then '" + _values[i+1] + "' ";
        }
        returned_update_sql_query += "END) WHERE column_name IN ('";
        for (unsigned int i = 0; i < _values.size(); i = i + 2)
        {
            returned_update_sql_query += _values[i] + "', '";
        }
        returned_update_sql_query = returned_update_sql_query.substr(0, returned_update_sql_query.length() - 4);
        returned_update_sql_query += "');";
    }
    else
    {
        returned_update_sql_query += "'" + _values[0] + "' WHERE column_name = '" + _values[1] + "';";
    }
    return returned_update_sql_query;
}

string create_insert_sql_query(string _table, string _key, vector<string> _columns,  vector<string> _values)
{
    string returned_insert_sql_query =  "START TRANSACTION; "
                                        "CREATE TABLE " + _key + " (column_name varchar(255), value varchar(255)); ";
    for (unsigned int i = 1; i < _values.size(); i++)
    {
        returned_insert_sql_query += "INSERT INTO " + _key + " (column_name, value) VALUES('" + _columns[i] + "', '" + _values[i] + "'); ";
    }
    returned_insert_sql_query += "COMMIT;";

    return returned_insert_sql_query;
}

string create_delete_sql_query(string _table, string _key)
{
    string returned_delete_sql_query = "DROP TABLE " + _key + " ;";

    return returned_delete_sql_query;
}

vector<string> extract_select_data(string _select_clause)
{
    string select_clause_data = _select_clause.substr(6, std::string::npos);
    vector<string> returned_vector;

    size_t pos = 0;
    string token;

    while ((pos = select_clause_data.find(',')) != std::string::npos)
    {
        token = select_clause_data.substr(0, pos);
        if (token.find('.') != std::string::npos)
        {
            token = token.substr(token.find('.') + 1);
        }
        token.erase(remove(token.begin(), token.end(), '('), token.end());
        token.erase(remove(token.begin(), token.end(), '\''), token.end());
        token.erase(remove(token.begin(), token.end(), ' '), token.end());
        token.erase(remove(token.begin(), token.end(), ';'), token.end());
        token.erase(remove(token.begin(), token.end(), ')'), token.end());

        returned_vector.push_back(token);
        select_clause_data.erase(0, pos + 1);
    }

    if (select_clause_data.find('.') != std::string::npos)
    {
        select_clause_data = select_clause_data.substr(select_clause_data.find('.') + 1);
    }
    select_clause_data.erase(remove(select_clause_data.begin(), select_clause_data.end(), '('), select_clause_data.end());
    select_clause_data.erase(remove(select_clause_data.begin(), select_clause_data.end(), '\''), select_clause_data.end());
    select_clause_data.erase(remove(select_clause_data.begin(), select_clause_data.end(), ' '), select_clause_data.end());
    select_clause_data.erase(remove(select_clause_data.begin(), select_clause_data.end(), ';'), select_clause_data.end());
    select_clause_data.erase(remove(select_clause_data.begin(), select_clause_data.end(), ')'), select_clause_data.end());

    returned_vector.push_back(select_clause_data);

    return returned_vector;
}

string extract_from_data(string _form_clause)
{
    string form_clause_data = _form_clause.substr(5, std::string::npos);

    form_clause_data.erase(remove(form_clause_data.begin(), form_clause_data.end(), '('), form_clause_data.end());
    form_clause_data.erase(remove(form_clause_data.begin(), form_clause_data.end(), '\''), form_clause_data.end());
    form_clause_data.erase(remove(form_clause_data.begin(), form_clause_data.end(), ';'), form_clause_data.end());
    form_clause_data.erase(remove(form_clause_data.begin(), form_clause_data.end(), ' '), form_clause_data.end());
    form_clause_data.erase(remove(form_clause_data.begin(), form_clause_data.end(), ')'), form_clause_data.end());

    return form_clause_data;
}

string extract_where_data(string _where_clause)
{
    string where_clause_data = _where_clause.substr(6, std::string::npos);

    size_t pos = 0;

    where_clause_data.erase(remove(where_clause_data.begin(), where_clause_data.end(), '('), where_clause_data.end());
    where_clause_data.erase(remove(where_clause_data.begin(), where_clause_data.end(), '\''), where_clause_data.end());
    where_clause_data.erase(remove(where_clause_data.begin(), where_clause_data.end(), ' '), where_clause_data.end());
    where_clause_data.erase(remove(where_clause_data.begin(), where_clause_data.end(), ';'), where_clause_data.end());
    where_clause_data.erase(remove(where_clause_data.begin(), where_clause_data.end(), ')'), where_clause_data.end());

    while ((pos = where_clause_data.find('=')) != std::string::npos)
    {
        where_clause_data.erase(0, pos + 1);
    }

    return where_clause_data;
}

string extract_update_data(string _update_clause)
{
    string update_clause_data = _update_clause.substr(7, std::string::npos);

    update_clause_data.erase(remove(update_clause_data.begin(), update_clause_data.end(), '('), update_clause_data.end());
    update_clause_data.erase(remove(update_clause_data.begin(), update_clause_data.end(), '\''), update_clause_data.end());
    update_clause_data.erase(remove(update_clause_data.begin(), update_clause_data.end(), ' '), update_clause_data.end());
    update_clause_data.erase(remove(update_clause_data.begin(), update_clause_data.end(), ';'), update_clause_data.end());
    update_clause_data.erase(remove(update_clause_data.begin(), update_clause_data.end(), ')'), update_clause_data.end());

    return update_clause_data;
}

vector<string> extract_set_data(string _set_clause)
{
    string set_clause_data = _set_clause.substr(3, std::string::npos);
    vector<string> returned_vector;

    size_t pos = 0;
    size_t pos_bis = 0;
    string token;
    string token_bis;

    while ((pos = set_clause_data.find(',')) != std::string::npos)
    {
        token = set_clause_data.substr(0, pos);

        while ((pos_bis = token.find('=')) != std::string::npos)
        {
            token_bis = token.substr(0, pos_bis);
            if (token_bis.find('.') != std::string::npos)
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

    while ((pos = set_clause_data.find('=')) != std::string::npos)
    {
        token = set_clause_data.substr(0, pos);
        if (token.find('.') != std::string::npos)
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

string extract_insert_into_data_table(string _insert_into_clause)
{
    string insert_into_clause_data = _insert_into_clause.substr(12, std::string::npos);

    insert_into_clause_data = insert_into_clause_data.substr(0, insert_into_clause_data.find("("));

    insert_into_clause_data.erase(remove(insert_into_clause_data.begin(), insert_into_clause_data.end(), '('), insert_into_clause_data.end());
    insert_into_clause_data.erase(remove(insert_into_clause_data.begin(), insert_into_clause_data.end(), '\''), insert_into_clause_data.end());
    insert_into_clause_data.erase(remove(insert_into_clause_data.begin(), insert_into_clause_data.end(), ' '), insert_into_clause_data.end());
    insert_into_clause_data.erase(remove(insert_into_clause_data.begin(), insert_into_clause_data.end(), ';'), insert_into_clause_data.end());
    insert_into_clause_data.erase(remove(insert_into_clause_data.begin(), insert_into_clause_data.end(), ')'), insert_into_clause_data.end());

    return insert_into_clause_data;
}

vector<string> extract_insert_into_data_columns(string _insert_into_clause)
{
    string insert_into_clause_data = _insert_into_clause.substr(12, std::string::npos);
    vector<string> returned_vector;

    size_t pos = 0;
    string token;

    insert_into_clause_data.erase(0, insert_into_clause_data.find("("));

    while ((pos = insert_into_clause_data.find(',')) != std::string::npos)
    {
        token = insert_into_clause_data.substr(0, pos);
        if (token.find('.') != std::string::npos)
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

    if (insert_into_clause_data.find('.') != std::string::npos)
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

vector<string> extract_values_data(string _values_clause)
{
    string values_clause_data = _values_clause.substr(7, std::string::npos);
    vector<string> returned_vector;

    size_t pos = 0;
    string token;

    while ((pos = values_clause_data.find(',')) != std::string::npos)
    {
        token = values_clause_data.substr(0, pos);
        if (token.find('.') != std::string::npos)
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

    if (values_clause_data.find('.') != std::string::npos)
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

string extract_delete_data(string &_delete_clause)
{
    string delete_clause_data = _delete_clause.substr(12, std::string::npos);

    delete_clause_data.erase(remove(delete_clause_data.begin(), delete_clause_data.end(), '('), delete_clause_data.end());
    delete_clause_data.erase(remove(delete_clause_data.begin(), delete_clause_data.end(), '\''), delete_clause_data.end());
    delete_clause_data.erase(remove(delete_clause_data.begin(), delete_clause_data.end(), ' '), delete_clause_data.end());
    delete_clause_data.erase(remove(delete_clause_data.begin(), delete_clause_data.end(), ';'), delete_clause_data.end());
    delete_clause_data.erase(remove(delete_clause_data.begin(), delete_clause_data.end(), ')'), delete_clause_data.end());

    return delete_clause_data;
}

string CQLtoSQL(string _incoming_cql_query)
{
    const string select_clauses[4] = { "SELECT ", "FROM ", "WHERE ", "LIMIT " };
    const string update_clauses[3] = { "UPDATE ", "SET ", "WHERE " };
    const string insert_into_clauses[2] = { "INSERT INTO ", "VALUES " };
    const string delete_clauses[2] = { "DELETE ", "WHERE " };

    string select_sub, from_sub, where_sub, update_sub, set_sub, insert_into_sub, values_sub, delete_sub = "";
    size_t from_sub_pos, where_sub_pos, limit_sub_pos, update_sub_pos, set_sub_pos, insert_into_sub_pos, values_sub_pos, delete_sub_pos = 0;

    string select_clause, from_clause, where_clause, update_clause, set_clause, insert_into_clause, values_clause, delete_clause = "";
    
    string table, key = "";
    vector<string> fields, values, columns;


    if (_incoming_cql_query.substr(0, 6) == "SELECT" || _incoming_cql_query.substr(0, 6) == "select")
    {
        cout << "Type de la requete : SELECT" << endl;
        from_sub_pos = _incoming_cql_query.find(select_clauses[1]);
        where_sub_pos = _incoming_cql_query.find(select_clauses[2]);
        limit_sub_pos = _incoming_cql_query.find(select_clauses[3]);
        
        //On génère les clauses de la requête
        select_clause = _incoming_cql_query.substr(6, from_sub_pos-6);
        table = _incoming_cql_query.substr(from_sub_pos+4, where_sub_pos-from_sub_pos-4);
        key = _incoming_cql_query.substr(where_sub_pos+5, limit_sub_pos+where_sub_pos-5);
        fields = extract_select_data(select_clause);

        //Affichage des paramètres
        cout << "Table: " << table << " - Key: " << key << " Fields : ";
        for (string field : fields)
        {
            cout << field << " __ ";
        }
        cout << endl;

        //Appel de la fonction de conversion pour du RWCS
        return create_select_sql_query(table, key, fields);
    }

    //Si requête UPDATE:
    else if (_incoming_cql_query.find("UPDATE ") != std::string::npos)
    {
        cout << "Type de la requete : UPDATE" << endl;
        update_sub_pos = _incoming_cql_query.find(update_clauses[0]);
        set_sub_pos = _incoming_cql_query.find(update_clauses[1]);
        where_sub_pos = _incoming_cql_query.find(update_clauses[2]);

            //On génère les clauses de la requête
            update_clause = _incoming_cql_query.substr(update_sub_pos, set_sub_pos - update_sub_pos);
            set_clause = _incoming_cql_query.substr(set_sub_pos, where_sub_pos - set_sub_pos);
            where_clause = _incoming_cql_query.substr(where_sub_pos, std::string::npos);
                //On extrait les paramètres des clauses
                table = extract_update_data(update_clause);
                key = extract_where_data(where_clause);
                values = extract_set_data(set_clause);

        //Affichage des paramètres
        cout << "Table: " << table << " - Key: " << key << " Fields : ";
        for (unsigned int i = 0; i < values.size(); i = i + 2)
        {
            cout << values[i] << " => " << values[i+1] << " __ ";
        }
        cout << endl;

        //Appel de la fonction de conversion pour du RWCS
        return create_update_sql_query(table, key, values);
    }

    //Si requête INSERT:
    else if (_incoming_cql_query.find("INSERT ") != std::string::npos)
    {
        cout << "Type de la requete : INSERT" << endl;
        insert_into_sub_pos = _incoming_cql_query.find(insert_into_clauses[0]);
        values_sub_pos = _incoming_cql_query.find(insert_into_clauses[1]);

            //On génère les clauses de la requête
            insert_into_clause = _incoming_cql_query.substr(insert_into_sub_pos, values_sub_pos - insert_into_sub_pos);
            values_clause = _incoming_cql_query.substr(values_sub_pos, std::string::npos);

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

        //Appel de la fonction de conversion pour du RWCS
        return create_insert_sql_query(table, key, columns, values);
    }

    //Si requête DELETE:
    else if (_incoming_cql_query.find("DELETE ") != std::string::npos)
    {
        cout << "Type de la requete : DELETE" << endl;
        delete_sub_pos = _incoming_cql_query.find(delete_clauses[0]);
        where_sub_pos = _incoming_cql_query.find(delete_clauses[1]);
            //On génère les clauses de la requête
            delete_clause = _incoming_cql_query.substr(delete_sub_pos, where_sub_pos - delete_sub_pos);
            where_clause = _incoming_cql_query.substr(where_sub_pos, std::string::npos);
                //On extrait les paramètres des clauses
                //table = extract_delete_data(delete_clause);
                key = extract_where_data(where_clause);
                
        //Affichage des paramètres
        cout << "Table: " << table << " - Key: " << key << endl;

        //Appel de la fonction de conversion pour du RWCS
        return create_delete_sql_query(table, key);
    }
    else{
        logs("");
        exit(EXIT_FAILURE);
    }
}
#pragma endregion CQL_SQL



#pragma region Utils

void logs(const char *msg)
{
    timestamp = gmtime(&t);
    strftime(date, sizeof(date), "%Y%m%d_%H:%M:%S", timestamp);
    printf("log.%s\n", date);
    perror(msg);
}

void exit_prog(int codeEXIT){

    exit(codeEXIT);
}

#pragma endregion Utils



#pragma region Server_connection
int connect_to_server(server _server_to_connect, int _port_to_connect)
{
    int sock_to_server = 0;
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
    while (1)
    {
        //On hash la clé extraite de la requête via la fonction string_Hashing()
        if (l_bufferRequests.size() > 0)
        {
            string key_from_cql_query = key_extractor(l_bufferRequests.back().request);

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
                    send_to_server(socket_neighbor_1, l_bufferRequests.back().request);
                }
                /*else if (server_to_redirect.server_id == neighbor_server_2.server_id)
                {
                    cout << "Requete a rediriger vers le voisin" << server_to_redirect.server_name << endl;
                    send_to_server(socket_neighbor_2, l_bufferRequests.back().request);
                }*/
                //Si non on crée la connection et on envoie
                else
                {
                    cout << "Requete a rediriger vers " << server_to_redirect.server_name << endl;
                    send_to_server(connect_to_server(server_to_redirect, port), l_bufferRequests.back().request);
                }
            }
            else
            {
                //Envoi vers PostgreSQL
                l_bufferRequestsForActualServer.push_front(incoming_cql_query);
                cout << "Requete a envoyer vers PostgreSQL" << endl;
            }
            key_from_cql_query = "";
            l_bufferRequests.pop_back();
        }

        if (l_bufferRequestsFromServer.size() > 0)
        {
            string key_from_cql_query = key_extractor(l_bufferRequestsFromServer.back());

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
                    send_to_server(socket_neighbor_1, l_bufferRequestsFromServer.back());
                }
                /*else if (server_to_redirect.server_id == neighbor_server_2.server_id)
                {
                    cout << "Requete a rediriger vers le voisin" << server_to_redirect.server_name << endl;
                    send_to_server(socket_neighbor_2, l_bufferRequestsFromServer.back());
                }*/
                //Si non on crée la connection et on envoie
                else
                {
                    cout << "Requete a rediriger vers " << server_to_redirect.server_name << endl;
                    send_to_server(connect_to_server(server_to_redirect, port), l_bufferRequestsFromServer.back());
                }
            }
            else
            {
                //Envoi vers PostgreSQL
                l_bufferRequestsForActualServer.push_front(incoming_cql_query);
                cout << "Requete a envoyer vers PostgreSQL" << endl;
            }
            key_from_cql_query = "";
            l_bufferRequestsFromServer.pop_back();
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
        logs("");
        exit(EXIT_FAILURE);
    }
}
#pragma endregion Redirecting