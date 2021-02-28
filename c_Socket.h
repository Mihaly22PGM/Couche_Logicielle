typedef int SOCKET;

extern unsigned char f_connexion[][15000];
extern int f_length[];
extern SOCKET listenfd;
extern SOCKET connfd;
extern SOCKET connfd2;
extern int portClients;
extern char buffer[1024];
extern char buffer2[1024];

extern struct sockaddr_in serv_addr; 

extern void INITSocket();
extern void SYN_responsesA();
extern void TraitementFrame(char[10240]);
