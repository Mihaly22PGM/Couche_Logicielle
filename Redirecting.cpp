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

using namespace std;

struct server
{
    string server_name;
    int server_id;
    string server_ip_address;
};

int string_Hashing(string _incoming_cql_query)
{
    hash<string> hash_string;
    int returned_hashed_key = hash_string(_incoming_cql_query);

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
        if (!ifa->ifa_addr) {
            continue;
        }
        if (ifa->ifa_addr->sa_family == AF_INET) { // check it is IP4
            // is a valid IP4 Address
            tmpAddrPtr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            //printf("%s IPV4 Address %s\n", ifa->ifa_name, addressBuffer);
            if (strcmp(ifa->ifa_name, "eth0") == 0)
            {
                address = addressBuffer;
            }

        } /*else if (ifa->ifa_addr->sa_family == AF_INET6) { // check it is IP6
            // is a valid IP6 Address
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
    if (ifAddrStruct != NULL) freeifaddrs(ifAddrStruct);

    //cout << endl << "Mon adresse c'est: " << address << endl;

    return address;
}

int main()
{
    int server_count = 6;
    server actual_server;

    int sockServer = 0;
    struct sockaddr_in serv_addr;
    int port = 9042;

    server server_A = { "RWCS-vServer1", 1, "192.168.82.63" };
    server server_B = { "RWCS-vServer2", 2, "192.168.82.64" };
    server server_C = { "RWCS-vServer3", 3, "192.168.82.55" };
    server server_D = { "RWCS-vServer4", 4, "192.168.82.56" };
    server server_E = { "RWCS-vServer5", 5, "192.168.82.58" };
    server server_F = { "RWCS-vServer6", 6, "192.168.82.59" };

    if (get_ip_from_actual_server() == server_A.server_ip_address)
        actual_server = server_A;

    else if (get_ip_from_actual_server() == server_B.server_ip_address)
        actual_server = server_B;

    else if (get_ip_from_actual_server() == server_C.server_ip_address)
        actual_server = server_C;

    else if (get_ip_from_actual_server() == server_D.server_ip_address)
        actual_server = server_D;

    else if (get_ip_from_actual_server() == server_E.server_ip_address)
        actual_server = server_E;

    else if (get_ip_from_actual_server() == server_F.server_ip_address)
        actual_server = server_F;

    cout << "Serveur #" << actual_server.server_id << ", Nom : " << actual_server.server_name << ", Adresse IP: " << actual_server.server_ip_address << endl;

    string incoming_cql_query = "abc";
    int hashed_key = string_Hashing(incoming_cql_query);
    cout << hashed_key << endl;

    if (hashed_key % server_count != actual_server.server_id)
    {
        //Redirection

        if (hashed_key % server_count == server_A.server_id)
        {
            /* code */
        }
        else if (hashed_key % server_count == server_B.server_id)
        {
            /* code */
        }
        else if (hashed_key % server_count == server_C.server_id)
        {
            /* code */
        }
        else if (hashed_key % server_count == server_D.server_id)
        {
            /* code */
        }
        else if (hashed_key % server_count == server_E.server_id)
        {
            /* code */
        }
        else if (hashed_key % server_count == server_F.server_id)
        {
            /* code */
        }
    }
    else
    {
        //Envoi vers PostgreSQL
    }
}