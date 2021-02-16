#include "Snif.h"

void Snif::run(const string& interface) {
        using std::placeholders::_1;
        sender_.default_interface(interface);           // Make the PacketSender use this interface by default     
        
        config.set_filter("tcp");               //Get only tcp
        config.set_immediate_mode(true);        //TODO : check if needed
        
        Sniffer sniffer(interface, config);     // Create the sniffer and start the capture
        sniffer.sniff_loop(bind(&tcp_connection_closer::callback, this, _1));
    }

bool Snif::callback(const PDU& pdu) {
        const EthernetII& eth = pdu.rfind_pdu<EthernetII>();
        const IP& ip = pdu.rfind_pdu<IP>();
        const TCP& tcp = pdu.rfind_pdu<TCP>();
        // We'll only close a connection when seeing a SYN|ACK
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
    }
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
