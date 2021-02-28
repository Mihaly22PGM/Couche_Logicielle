#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <thread>
#include "c_Socket.h"
#include <list>

typedef int SOCKET;

#pragma region Global
int server;
char opcode;
char buff[1024];
char longueur;
std::list<char*> l_bufferIN;
#pragma endregion Global

#pragma region Prototypes
void Traitement();
#pragma endregion Prototypes

int main(int argc, char *argv[])
{ 
    INITSocket();   //Sockets creation

        //ConnexionCQLSH();

        //Starting threads for sockets
        std::thread th_SYN(SYN_responsesA);
        std::thread th_Traitement(Traitement);

        while(1){
            server = 0;
            server = recv(connfd2, buffer2, sizeof(buffer2),0);
            if(server > 0){
                l_bufferIN.push_front(buffer2);
		printf("\r\n");
		printf("Pushed\r\n");
		printf("Longueur de liste : %d \r\n", l_bufferIN.size());
	    }
        }
        //Stop the threads
        th_SYN.join();
        th_Traitement.join();
    
    return 0;
}

#pragma region Fonctions
void Traitement(){
    while(1){
        if(l_bufferIN.size()>0){
            TraitementFrame(l_bufferIN.back());
            l_bufferIN.pop_back();
	    printf("Pop\r\n");
        }
    }
}
#pragma endregion Fonctions
