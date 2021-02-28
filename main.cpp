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
#include <list>
#include "c_Socket.h"

typedef int SOCKET;

#pragma region Global
char buffData[1024];
std::list<char*> l_bufferIN;
#pragma endregion Global

#pragma region Prototypes
void Traitement();
#pragma endregion Prototypes

int main(int argc, char *argv[])
{ 
    INITSocket();   //Sockets creation

        //Starting threads for sockets
        std::thread th_SYN(TraitementFrameClient);
        std::thread th_Traitement(Traitement);

        while(1){
            sockServer = 0;
            sockServer = recv(sockDataClient, buffData, sizeof(buffData),0);
            if(sockServer > 0){
                l_bufferIN.push_front(buffData);
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
            TraitementFrameDataClient(l_bufferIN.back());
            l_bufferIN.pop_back();
	    printf("Pop\r\n");
        }
    }
}
#pragma endregion Fonctions
