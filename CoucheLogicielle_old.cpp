#include "iostream"
#include "Snif.h"
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
//#include <tins/tins.h>

#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket(s) close (s)

typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;

#pragma region STRUCTURES
    struct Socket_Types
    {
        int domain;        	//Socket TCP/IP
        int type;     		//Protocole TCP IP
        int protocol;           //Socket par défaut
    };

#pragma endregion STRUCTURES

#pragma region DEFINITIONS_GLOBALES
    int server;
    int portClient = 9042;
    char buffer[32];
    char buff[32];
    socklen_t size;
    Socket_Types tcp_Socket;
    Socket_Types unix_Socket;
    SOCKADDR_IN csin;
    SOCKET csock;
    socklen_t recsize;

#pragma endregion DEFINITIONS_GLOBALES


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
    recsize = sizeof(csin);
    if (listen(socket, 10) != SOCKET_ERROR) /* Boucle infinie. Exercice : améliorez ce code. */
    {
        csock = accept(socket, (SOCKADDR*)&csin, &recsize);
        Snif snif;
	    snif.IPAddresse = inet_ntoa(csin.sin_addr);	//Récupération adresse
        snif.Port = htons(csin.sin_port);		//Récupération port
        snif.StartSniffer();
    }

    //C.disconnect();
    close(socket);     //Pas réellement nécessaire puisque boucle infinie juste avant

    return EXIT_SUCCESS;
}
