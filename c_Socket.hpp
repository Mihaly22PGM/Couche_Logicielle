#ifndef C_SOCKET_HPP
#define C_SOCKET_HPP

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
// #include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>

typedef int SOCKET;

SOCKET CreateSocket();                  //Socket creation
SOCKET INITSocket(SOCKET sockServer);   //Socket initialisation 
void *TraitementFrameClient(void*);     //Response to isalive requests
in_addr GetIPAdress();                  //Get the IP Adress of the server

#endif