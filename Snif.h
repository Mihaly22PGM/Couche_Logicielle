#ifndef DEF_SNIFFER
#define DEF_SNIFFER

#include <sys/socket.h>
#include <tins/tins.h>

typedef int PORT;
typedef struct sockaddr SOCKADDR;
typedef struct sockaddr_in SOCKADDR_IN;

class Snif
{
    public:
    char* IPAddresse;
    PORT Port;
    //void SetConfig(SOCKADDR* addr);
    void StartSniffer();

    private:
    SnifferConfiguration config;
    //SOCKADDR* IPAddresse;
    //PORT Port;

};

#endif
