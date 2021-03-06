#include <iostream>
#include <functional>
#include <stdio.h>      
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h> 
#include <string.h> 
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <thread>

using namespace std;

struct server
{
    string server_name;
    int server_id;
    string server_ip_address;
};

//Variables
int server_count = 6;
server actual_server;
server neighbor_server_1;
server neighbor_server_2;
server server_to_redirect;
int port = 8042;
int valread;
string key_from_cql_query;

//Liste des serveurs
server server_A = { "RWCS-vServer1", 1, "192.168.82.63" };
server server_B = { "RWCS-vServer2", 2, "192.168.82.64" };
server server_C = { "RWCS-vServer3", 3, "192.168.82.55" };
server server_D = { "RWCS-vServer4", 4, "192.168.82.56" };
server server_E = { "RWCS-vServer5", 5, "192.168.82.58" };
server server_F = { "RWCS-vServer6", 6, "192.168.82.59" };

int string_Hashing(string _key_from_cql_query)
{
    hash<string> hash_string;
    int returned_hashed_key = hash_string(_key_from_cql_query);

    if (returned_hashed_key < 0)
        returned_hashed_key = 0 - returned_hashed_key;

    return returned_hashed_key;
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
            //printf("%s IPV4 Address %s\n", ifa->ifa_name, addressBuffer);
            if (strcmp(ifa->ifa_name, "eth0") == 0)
                address = addressBuffer;

        } /*else if (ifa->ifa_addr->sa_family == AF_INET6) // check it is IP6
        {
            tmpAddrPtr=&((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
            char addressBuffer[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
            printf("%s IPV6 Address %s\n", ifa->ifa_name, addressBuffer);
            if (strcmp(ifa->ifa_name, "eth0") == 0)
            {
                address =  addressBuffer;
                cout << "coucou" << address << endl;
            }

        }*/
    }
    if (ifAddrStruct != NULL)
        freeifaddrs(ifAddrStruct);

    //cout << endl << "Mon adresse c'est: " << address << endl;

    return address;
}

int connect_to_server(server _server_to_connect, int _port_to_connect)
{
    int sock_to_server = 0;
    struct sockaddr_in serv_addr;
    const char* ip_address = _server_to_connect.server_ip_address.c_str();

    if ((sock_to_server = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        cout << endl << "sock_to_server: socket failed" << endl;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(_port_to_connect);

    /*for(int i = 0; i < _server_to_connect.server_ip_address.size(); i++)
        ip_address[i] = _server_to_connect.server_ip_address[i];*/

    if (inet_pton(AF_INET, ip_address, &serv_addr.sin_addr) <= 0)
        cout << endl << ip_address << endl << "sock_to_server: inet_pton failed" << endl;

    if (connect(sock_to_server, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
        cout << endl << "sock_to_server: connect failed" << endl;

    return sock_to_server;
}

void send_to_server(int _socketServer, string _query_to_send)
{
    const char* cql_query = _query_to_send.c_str();

    /*for(int i = 0; i < _query_to_send.size(); i++)
        cql_query[i] = _query_to_send[i];*/

    send(_socketServer, cql_query, strlen(cql_query), 0);
    cout << endl << "Incoming query sent" << endl;
}

void waiting_for_client()
{
    int socket_for_client, client_connection;
    struct sockaddr_in address;

    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[1024] = { 0 };

    if ((socket_for_client = socket(AF_INET, SOCK_STREAM, 0)) == 0)
        cout << endl << "socket_for_client: socket error" << endl;

    if (setsockopt(socket_for_client, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
        cout << endl << "socket_for_client: setsockopt failed" << endl;

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(socket_for_client, (struct sockaddr*)&address, sizeof(address)) < 0)
        cout << endl << "socket_for_client: bind failed" << endl;

    if (listen(socket_for_client, 10) < 0)
        cout << endl << "socket_for_client: listen failed" << endl;

    while (1)
    {
        if ((client_connection = accept(socket_for_client, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0)
            cout << endl << "socket_for_client: accept failed" << endl;

        valread = read(client_connection, buffer, 1024);
        cout << endl << buffer << endl;
    }
}

void redirecting()
{
    while (1)
    {
        //On hash la clé extraite de la requête via la fonction string_Hashing()
        if (key_from_cql_query != "")
        {
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
                /*if (server_to_redirect.server_id == neighbor_server_1.server_id)              //...
                {
                    cout << "Requete a rediriger vers " << server_to_redirect.server_name << endl;
                    send_to_server(socket_neighbor_1, key_from_cql_query);
                }
                else if (server_to_redirect.server_id == neighbor_server_2.server_id)
                {
                    cout << "Requete a rediriger vers " << server_to_redirect.server_name << endl;
                    send_to_server(socket_neighbor_2, key_from_cql_query);
                }
                //Si non on crée la connection et on envoie
                else
                {*/
                cout << "Requete a rediriger vers " << server_to_redirect.server_name << endl;
                send_to_server(connect_to_server(server_to_redirect, port), key_from_cql_query);
                //}
            }
            else
            {
                //Envoi vers PostgreSQL
                cout << "Requete a envoyer vers PostgreSQL" << endl;
            }
            key_from_cql_query = "";
        }
    }
}

int main()
{
    cout << "coucou" << endl;

    //On récupère l'IP via la fonction get_ip_from_actual_server() et on la compare avec les adresses des serveurs pour savoir sur quel "server" on est
    //On définit aussi les serveurs "voisins" vers lesquels on va établir des connexions par socket permanentes
    //Si on doit rediriger vers un serveur non voisin, on créera la connexion à la volée
    if (get_ip_from_actual_server() == server_A.server_ip_address)
    {
        actual_server = server_A;
        neighbor_server_1 = server_B;
        neighbor_server_2 = server_F;
    }

    else if (get_ip_from_actual_server() == server_B.server_ip_address)
    {
        actual_server = server_B;
        neighbor_server_1 = server_C;
        neighbor_server_2 = server_A;
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
    //Si on est sur le pc et pas sur un serveur             //..
    else
    {
        actual_server = { "PC", 80, "80.80.80.80" };
        neighbor_server_1 = { "PC+1", 81, "81.81.81.81" };
        neighbor_server_2 = { "PC-1", 79, "79.79.79.79" };
    }

    cout << get_ip_from_actual_server() << endl;
    cout << "Serveur #" << actual_server.server_id << ", Nom : " << actual_server.server_name << ", Adresse IP: " << actual_server.server_ip_address << endl;

    //On crée le socket de réception pour accepter les requêtes redirigées
    std::thread th_waiting_for_client(waiting_for_client);

    //On crée les connexions permanentes avec les serveurs voisins
    /*int socket_neighbor_1 = connect_to_server(neighbor_server_1, port);               //...
    int socket_neighbor_2 = connect_to_server(neighbor_server_2, port);*/

    // UNE FOIS AU DEBUT
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // A CHAQUE FOIS QU'ON RECOIT UNE REQUETE

        //Clé extraite de la requête
    key_from_cql_query = "abc";      //...

    std::thread th_redirecting(redirecting);

    th_waiting_for_client.join();
    th_redirecting.join();
    return 0;
}