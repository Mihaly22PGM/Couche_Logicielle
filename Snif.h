#ifndef DEF_SNIFFER
#define DEF_SNIFFER

#include <iostream>
#include <string>
#include <functional>
#include <tins/tins.h>

//#include <sys/socket.h>

/*typedef int PORT;
typedef struct sockaddr SOCKADDR;
typedef struct sockaddr_in SOCKADDR_IN;*/

using std::string;
using std::bind;
using std::exception;

using namespace Tins;

class Snif {
public:
    SnifferConfiguration config;    // Create the sniffer configuration

    tcp_connection_closer() {
    }

    void run(const string& interface)

private:
    bool callback(const PDU& pdu)

    PacketSender sender_;
};
/*class Snif
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

};*/

#endif
