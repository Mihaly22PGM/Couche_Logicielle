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

int main(int argc, char *argv[])
{
    int CheckThreadCreation = 0;
    INITSocket();   //Sockets creation
    ConnexionPGSQL();   //Connexion PGSQL

    //Starting threads for sockets
    CheckThreadCreation += pthread_create(&th_FrameClient, NULL, TraitementFrameClient, NULL);
    CheckThreadCreation += pthread_create(&th_FrameData, NULL, TraitementFrameData, NULL);
    CheckThreadCreation += pthread_create(&th_Requests, NULL, TraitementRequests, NULL);
    CheckThreadCreation += pthread_create(&th_PostgreSQL, NULL, SendPGSQL, NULL);
    
    //Check if threads have been created
    if (CheckThreadCreation !=0){
        logs("main() : Error while creating threads, abording");
        exit_prog(EXIT_FAILURE);
    }
    
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

void* TraitementRequests(void *arg){
    while(1){
        if(l_bufferRequests.size()>0){
            translated_sql_query = CQLtoSQL(l_bufferRequests.back().request);
            l_bufferRequests.pop_back(); 
            l_bufferPGSQLRequests.push_front(translated_sql_query);
        }
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
        fprintf(stderr, "Connection to database failed: %s", PQerrorMessage(conn));
        logs("ConnexionPGSQL() : Connexion to database failed");
        exit_prog(EXIT_FAILURE);
    }

    /* Set always-secure search path, so malicious users can't take control. */
    res = PQexec(conn, "SELECT pg_catalog.set_config('search_path', 'public', false)");
    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        fprintf(stderr, "SET failed: %s", PQerrorMessage(conn));
        PQclear(res);   //TODO check what is it?
        logs("ConnexionPGSQL() : Secure search path error");
        exit_prog(EXIT_FAILURE); 
    }
    else{
	      std::cout<<"Connexion PGSQL OKAYYYY"<<endl;
    }
    PQclear(res);
}

void *SendPGSQL(void *arg){
    PGresult *res;
    string requestPGSQL;
    while(1){
        if(l_bufferPGSQLRequests.size() > 0){
            requestPGSQL = l_bufferPGSQLRequests.back();
            l_bufferPGSQLRequests.pop_back();      
            res = PQexec(conn, "SELECT * FROM test;");

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
            }
            else{
                cout<<"Erreur dans la requête"<<endl;
            }
        }
    }
}

#pragma endregion PostgreSQL

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
    string returned_insert_sql_query =  "CREATE TABLE " + _key + " (column_name varchar(255), value varchar(255)); ";
    for (unsigned int i = 1; i < _values.size(); i++)
    {
        returned_insert_sql_query += "INSERT INTO " + _key + " (column_name, value) VALUES('" + _columns[i] + "', '" + _values[i] + "'); ";
    }

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
