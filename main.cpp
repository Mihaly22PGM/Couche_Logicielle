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

typedef int SOCKET;

int main(int argc, char *argv[])
{ 
    //Sockets creation
    INITSocket();

    //Starting threads for sockets
    std::thread th_SYN(SYN_responsesA);
    std::thread th_SYN2(SYN_responsesB);
    printf("Listen Socket : %d\r\n",listenfd);
    printf("Socket Client : %d\r\n",connfd);
    printf("Socket Client 2 : %d\r\n",connfd2);
    while(1){

    }; //Boucle infinie

    //Stop the threads
    th_SYN.join();
    th_SYN2.join();
    
    return 0;
}


