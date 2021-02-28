typedef int SOCKET;

extern unsigned char f_connexion[][15000];
extern int f_length[];
extern SOCKET sockServer;
extern SOCKET sockClient;
extern SOCKET sockDataClient;
extern int portClients;
extern char buffClient[1024];

extern struct sockaddr_in serv_addr; 

extern void INITSocket();
extern void TraitementFrameClient();
extern void TraitementFrameDataClient(char[10240]);
