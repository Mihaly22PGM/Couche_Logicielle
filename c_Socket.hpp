#ifndef C_SOCKET_HPP
#define C_SOCKET_HPP

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <errno.h>
#include <pthread.h>
#include <future>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <fcntl.h> 

typedef int SOCKET;

SOCKET CreateSocket();                  //Socket creation
SOCKET INITSocket(SOCKET, bool);        //Socket initialisation 
void *TraitementFrameClient(void*);     //Response to isalive requests
in_addr GetIPAdress();                  //Get the IP Adress of the server
SOCKET GetSocket();                     //Return data socket
SOCKET GetSocketConn();                 //Return conn socket
void StopSocketThread();                //Stopping threads
#endif