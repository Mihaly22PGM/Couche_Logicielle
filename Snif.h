#ifndef DEF_SNIFFER
#define DEF_SNIFFER

#include <iostream>
#include <string>
#include <functional>
#include <tins/tins.h>
#include <vector>

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
    
    Snif();
    std::vector<Packet> vt;
    void run(const string& interface);
    void readTCP();
    typedef struct Trame Trame;
    struct Trame
{
	PDU& pdu;
	Trame *suivant;
}
typedef struct ListeTrames ListeTrames;
struct ListeTrames
{
	Trame* premier;
}


private:
    bool callback(const PDU& pdu);
    
    SnifferConfiguration config;
    PacketSender sender_;
    ListeTrames* listetrames = new ListeTrames;
    int CompteurTrames;
    typedef struct Trame Trame;
    struct Trame
    {
    	PDU& pdu;
    	Trame *suivant;
    };

    typedef struct ListeTrames ListeTrame;
    struct ListeTrames
    {
    	Trame* premier;
    };

    //ListeTrame* initialisation();

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
