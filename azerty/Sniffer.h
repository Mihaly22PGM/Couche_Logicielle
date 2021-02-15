#ifndef DEF_SNIFFER
#define DEF_SNIFFER

#include <sys/socket.h>

typedef int PORT;
typedef struct sockaddr SOCKADDR;
typedef struct sockaddr_in SOCKADDR_IN;

class Sniffer
{
    public:
    char* IPAddresse;
    PORT Port;
    void SetConfig(SOCKADDR* addr);

    private:
    //SOCKADDR* IPAddresse;
    //PORT Port;

};

#endif
