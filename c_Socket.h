#include <cstring>

typedef int SOCKET;

extern unsigned char f_connexion[][15000];
extern int f_length[];
extern SOCKET sockServer;
extern SOCKET sockClient;
extern SOCKET sockDataClient;
extern int portClients;
extern char buffClient[1024];

struct Request{
    char RequestNumber[2];
    char RequestOpcode;
    char Request[1024];
};

extern struct sockaddr_in serv_addr; 

extern void INITSocket();
extern void TraitementFrameClient();
extern Request TraitementFrameDataClient(char[10240]);
