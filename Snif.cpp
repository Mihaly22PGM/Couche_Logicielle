#include "Snif.h"
//#include <tins/tins.h>
//#include <sys/socket.h>

void Sniffer::StartSniffer(){

    config.set_filter("port 9042");
    sniffer.set_filter("ip src " +IPAddresse);
    config.set_promisc_mode(true);
    config.set_snap_len(400);
    Sniffer sniffer("eth0", config);
    PDU *some_pdu = sniffer.next_packet();
    std::cout<<some_pdu<<std::endl;
    return;
}

/*void Sniffer::SetConfig(&addr){
    //IPAdrresse = addr;
    port = 9042;
    return;
}*/
