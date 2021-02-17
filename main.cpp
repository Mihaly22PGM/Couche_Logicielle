#include "iostream"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <future>

using std::string;
using std::bind;
using std::exception;
using namespace Tins;

typedef struct Trame Trame;
typedef struct ListeTrames ListeTrames;

#pragma region Classes
class Snif {

public:

    Snif(){}; //Constructor

    struct Trame
    {
        PDU& pdu;
        Trame *suivant;
    }
    struct ListeTrames
    {
        Trame* premier;
    }

    ListeTrames* listetrames;

    void run(const string& interface, ListeTrames* liste){
        //using std::placeholders::_1;
        //sender_.default_interface(interface);           // Make the PacketSender use this interface by default     
        config.set_filter("tcp");               //Get only tcp
	    //config.set_filter("")
        config.set_immediate_mode(true);        //TODO : check if needed
        //config.set_promisc_mode(true);
	    std::cout<<"Setup"<<std::endl;
        Sniffer sniffer(interface, config);     // Create the sniffer and start the capture
        //sniffer.sniff_loop(make_sniffer_handler(this, &Snif::handle)));
   	    while(vt.size()<1000000){
		    insertion(liste, sniffer.next_packet());
	}
    };
    void readTCP(ListeTrames* liste){
        if (liste->premier != NULL){
	        PDU & pdu = liste->premier;
	        std::cout << "At: " << packet.timestamp().seconds()
                << " - " << packet.pdu()->rfind_pdu<IP>().src_addr() 
                << std::endl;
	    }
	    return;
    };
    ListeTrames* initialisation(){
        listetrames = new ListeTrames;
        if (listetrames == NULL)
            exit(EXIT_FAILURE);
        listetrames->premier = NULL
        return listetrames;
    };
    void insertion(ListeTrames* liste, PDU& pdu){
        Trame* newTrame = new Trame;
        if (listetrames == NULL || newtrame == NULL)
            exit(EXIT_FAILURE);
        newTrame->trame = &pdu;
        newTrame->suivant = listetrames->premier;
        listetrames->premier = newTrame;
    };
    void suppression(ListeTrames* liste){
        if (liste == NULL)
            exit(EXIT_FAILURE);
        if (liste->premier != NULL)
        {
            Trame* trameToDelete = listetrames->premier;
            listetrames->premier = listetrames->premier->suivant;
            free(trameToDelete);
        }
    };

private:
    SnifferConfiguration config;
    std::vector<Packet> vt;
};
#pragma endregion Classes

#pragma region Global

#pragma endregion Global

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: " << *argv << " <interface>" << std::endl;
        return 1;
    }
    try {
        Snif snif = new Snif;
        std::async(std::launch::async, &Snif::run, &snif, argv[1]);	//Lancement du sniffer en asyncrone
        while(1){
            snif.readTCP(liste);
            snif.suppression(liste);
        }	
    }
    catch (exception& ex) {
        std::cout << "[-] Error: " << ex.what() << std::endl;
	return 1;
    }
	return 0;
}
