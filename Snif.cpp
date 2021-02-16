#include "Snif.h"

/*#include <iostream>
#include <string>
#include <functional>
#include <tins/tins.h>

using std::string;
using std::bind;
using std::exception;

using namespace Tins;
*/
Snif::Snif(){}
	
ListeTrames* Snif::initialisation(){
	//ListeTrames* listetrames = new ListeTrames;

	if (liste == NULL)
    	{
        	exit(EXIT_FAILURE);
    	}
	listetrames->premier = NULL
	return liste;
}

void Snif::insertion(Liste* liste, PDU& pdu)
{
    Trame* newTrame = new Trame;
    if (listetrames == NULL || newtrame == NULL)
    {
        exit(EXIT_FAILURE);
    }

    newTrame->trame = &pdu;
    newTrame->suivant = listetrames->premier;
    listetrames->premier = newTrame;
   
}

void Snif::suppression(Liste* liste)
{
    if (liste == NULL)
    {
        exit(EXIT_FAILURE);
    }

    if (liste->premier != NULL)
    {
        Trame* trameToDelete = listetrames->premier;
        listetrames->premier = listetrames->premier->suivant;
        free(trameToDelete);
    }
}

void Snif::run(const string& interface, ListeTrames liste) {
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
   }

/*void Snif::readTCP(){
	if (vt.size()>0){
	PDU & pdu = vt.front();
	std::cout << "At: " << packet.timestamp().seconds()
                    << " - " << packet.pdu()->rfind_pdu<IP>().src_addr() 
                    << std::endl;
	}
	return;
}*/


/*bool handle(PDU& pdu){
	const TCP& tcp= pdu.rfind_pdu<TCP>();
	
}*/

/*bool Snif::callback(const PDU& pdu) {
        const EthernetII& eth = pdu.rfind_pdu<EthernetII>();
        const IP& ip = pdu.rfind_pdu<IP>();
        const TCP& tcp = pdu.rfind_pdu<TCP>();
        // We'll only close a connection when seeing a SYN|ACK
	std::cout<<tcp.flags()<<std::endl;
        if (tcp.has_flags(TCP::SYN | TCP::ACK)) {
            // Create an ethernet header flipping the addresses
            EthernetII packet(eth.src_addr(), eth.dst_addr());
            // Do the same for IP
	    std::cout<<eth.src_addr()<<std::endl;
            packet /= IP(ip.src_addr(), ip.dst_addr());
            // Flip TCP ports
            TCP response_tcp(tcp.sport(), tcp.dport());
            // Set RST|ACK flags
            response_tcp.flags(TCP::RST | TCP::ACK);
            // Use the right sequence and ack numbers
            response_tcp.seq(tcp.ack_seq());
            response_tcp.ack_seq(tcp.seq());
            // Add this PDU to the packet we'll send
            packet /= response_tcp;
            // Send it!
            sender_.send(packet);
        }
        return true;
    }*/
/*void Snif::StartSniffer(){

    config.set_filter("port 9042");
    sniffer.set_filter("ip src " +IPAddresse);
    config.set_promisc_mode(true);
    config.set_snap_len(400);
    Sniffer sniffer("eth0", config);
    PDU *some_pdu = sniffer.next_packet();
    std::cout<<some_pdu<<std::endl;
    return;
}*/

/*void Sniffer::SetConfig(&addr){
    //IPAdrresse = addr;
    port = 9042;
    return;
}*/
