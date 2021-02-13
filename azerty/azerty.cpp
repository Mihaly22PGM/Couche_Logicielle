#include <iostream>
#pragma comment(lib, "ws2_32.lib")
//#include <pqxx/pqxx>          //Telecharger sur https://github.com/jtv/libpqxx
//Linux socket librairies => erreurs sous windows
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>

using namespace std;

#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket(s) close (s)

typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;

#pragma region DEFINITION GLOBALES
    SOCKET socket;
    int server;
    int portClient = 23;
    char buffer[1024];
    socklen_t size;
    //sockaddr_in server_addr;
    //sockaddr_in client_addr;
#pragma endregion

#pragma region STRUCTURES
    struct tcp_Socket
    {
        int domain = AF_INET;        //Socket TCP/IP
        int type = SOCK_STREAM;     //Protocole TCP IP
        int protocol = 0;           //Socket par défaut
    };
    struct unix_Socket
    {
        int domain = AF_UNIX;        //Socket Unix
        int type = SOCK_STREAM;     //Protocole TCP IP
        int protocol = 0;           //Socket par défaut
    };
    struct sockaddr_in
    {
        short      sin_family;
        unsigned short   sin_port;
        struct   in_addr   sin_addr;
        char   sin_zero[8];
    };

#pragma endregion

SOCKET creationSocket(){
    SOCKET sock;
    sock = socket(tcp_Socket.domain, tcp_Socket.type, tcp_Socket.protocol);
    if (sock < 0)
    {
        cout << "\nError establishing socket..." << endl;
        exit(1);
    }
    else
        cout << "\n=> Socket server has been created..." << endl;
    
    //Déclaration du socket
    SOCKADDR_IN server_addr;
    //Initialisation du socket
    server_addr.sin_addr.s_addr = INADDR_ANY;           //Adresse de serveur
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(portClient);                   //Port d'écoute du client
    
    if ((bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr))) == SOCKET_ERROR)
    {
        cout << "=> Error binding connection, the socket has already been established..." << endl;
        return -1;
    }
    else
        cout << "=> Looking for clients..." << endl;
    //size = sizeof(server_addr);       //Pas utile? 
    return sock;
}


int main()
{
    //std::cout << "yolo" << std::endl;
    //int client, server;
    //int port = 23;
    //char buffer[1024];

    // Initialisation de WinSock
    

    //Creation du socket (SOCK_STREAM (TCP) ou SOCK_DGRAM (UDP))
    //client = socket(AF_INET, SOCK_STREAM, 0);
    /*if (client < 0)
    {
        cout << "\nError establishing socket..." << endl;
        exit(1);
    }
    cout << "\n=> Socket server has been created..." << endl;*/

    //Paramétrage du socket
    /*server_addr.sin_addr.s_addr = INADDR_ANY;           //Adresse de serveur
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);                   //Port*/
    
    /*if ((bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr))) == SOCKET_ERROR)
    {
        cout << "=> Error binding connection, the socket has already been established..." << endl;
        return -1;
    }

    size = sizeof(server_addr);
    cout << "=> Looking for clients..." << endl;
    listen(sock, 1);*/

    //Database => include pqxx/pqxx
    /*try
    {
        connection C ("dbname = testdb user = postgres password = cohondob hostaddr = 127.0.0.1 port = 5432");
        if (C.is_open())
        {
            std::cout << "Connexion a la base de donnees etablie: " << C.dbname() << std::endl;
        }
        else
        {
            std::cout << "Can't open database" << std::endl;
            return 1;
        }
    }
    catch (const std::exception& e)
    {
        cerr << e.what() << std::endl;
        return 1;
    }*/
    socket = creationSocket();
    while (listen(socket, 1) != SOCKET_ERROR) /* Boucle infinie. Exercice : améliorez ce code. */
    {
        //Réception de la requête du client
        server = recv(socket, buffer, sizeof(buffer), 0);

        if (server > 0)
        {
            //Traitement
            std::cout << buffer << std::endl;

            /*Envoi de la requête remaniée à la DB
            sql = "";
            if(SELECT...)
            {
                nontransaction N(C);

                result R( N.exec( sql ));

                for (result::const_iterator c = R.begin(); c != R.end(); ++c)
                {
                    cout << "ID = " << c[0].as<int>() << endl;
                    cout << "Name = " << c[1].as<string>() << endl;
                    cout << "Age = " << c[2].as<int>() << endl;
                    cout << "Address = " << c[3].as<string>() << endl;
                    cout << "Salary = " << c[4].as<float>() << endl;
                }
            }
            else
            {
                work W(C);

                W.exec( sql );
                W.commit();
            }
            */

            close(server);
        }
    }

    //C.disconnect();
    close(socket);     //Pas réellement nécessaire puisque boucle infinie juste avant

    return EXIT_SUCCESS;
}