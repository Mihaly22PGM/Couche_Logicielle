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


typedef struct Trame Trame;
typedef struct ListeTrames ListeTrames;

class Snif {

public:
    
    Snif(); //Constructor

    #pragma region Fonctions
    void run(const string& interface);
    void readTCP();
    ListeTrames* initialisation();
    void insertion();
    void suppression();
    #pragma endregion Fonctions

    ListeTrames* listetrames;

private:
    //bool callback(const PDU& pdu);
    
    SnifferConfiguration config;
    //PacketSender sender_;
    
    //int CompteurTrames;
    std::vector<Packet> vt;


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
