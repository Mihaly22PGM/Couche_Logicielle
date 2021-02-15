#include <sys/socket.h>
#include <string>

typedef int PORT; 
typedef struct sockaddr SOCKADDR;

class Sniffer
{
    public:
    void SetConfig(SOCKADDR addr);

    private:
    SOCKADDR IPAddresse;
    PORT port;

};

#endif