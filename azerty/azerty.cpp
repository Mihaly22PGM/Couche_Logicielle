#include "iostream"
#include <stdio.h>
//#include <pqxx/pqxx>          //Telecharger sur https://github.com/jtv/libpqxx
//Linux socket librairies => erreurs sous windows
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>

#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket(s) close (s)

typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;

#pragma region STRUCTURES
    struct Socket_Types
    {
        int domain;        //Socket TCP/IP
        int type;     //Protocole TCP IP
        int protocol;           //Socket par défaut
    };

    /*struct sockaddr_in
    {
        short      sin_family;
        unsigned short   sin_port;
        struct   in_addr   sin_addr;
        char   sin_zero[8];
    };*/

#pragma endregion

#pragma region DEFINITIONS GLOBALES
    int server;
    int portClient = 23;
    char buffer[1024];
    socklen_t size;
    Socket_Types tcp_Socket;
    Socket_Types unix_Socket;
    
#pragma endregion


SOCKET creationSocket(){
    SOCKET sock;

    //Création socket
    sock = socket(tcp_Socket.domain, tcp_Socket.type, tcp_Socket.protocol);
    
    //Si socket invalide
    if (sock == INVALID_SOCKET)
    {
        std::cout << "\nError establishing socket..." << std::endl;
        exit(EXIT_FAILURE);
    }
    else
        std::cout << "\n=> Socket server has been created..." << std::endl;       //DEBUG
    
    SOCKADDR_IN server_addr;        //Déclaration du socket

    //Initialisation du socket
    server_addr.sin_addr.s_addr = INADDR_ANY;                   //Adresse de serveur
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(portClient);                   //Port d'éstd::coute du client
    
    //Si connexion du socket engendre une erreur
    if ((bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr))) == SOCKET_ERROR)
    {
        std::cout << "=> Error binding connection, the socket has already been established..." << std::endl;
        return -1;
    }
    else
        std::cout << "=> Looking for clients..." << std::endl;
    return sock;
}


int main()
{
    //Paramétrage socket TCP (vers client)
    tcp_Socket.domain = AF_INET;    //Socket TCP/IP
    tcp_Socket.type = SOCK_STREAM;
    tcp_Socket.protocol = 0;
    //Paramétrage socket Unix (vers DB)
    unix_Socket.domain = AF_UNIX;   //SOCKET Unix
    unix_Socket.type = SOCK_STREAM;
    unix_Socket.protocol = 0;

    SOCKET socket = creationSocket();
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