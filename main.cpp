#include "Snif.h"

#include "iostream"
//#include <stdio.h>
//#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
//#include <stdlib.h>
//#include <unistd.h>
#include <thread>
#include <future>

using namespace std;

Snif snif;

//std::thread th_Sniffer;

/*void CloseThreads(){
	th_Sniffer.join();
	return;
}*/

/*void StartSniffer(char* argv[]){
	snif.run(argv[1]);
	return;
}*/

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: " << *argv << " <interface>" << std::endl;
        return 1;
    }
    try {
	std::async(std::launch::async, &Snif::run, &snif, argv[1]);	//Lancement du sniffer en asyncrone
	
	//Snif snif
        //snif.run(argv[]);
    	
    }
    catch (exception& ex) {
        std::cout << "[-] Error: " << ex.what() << std::endl;
        //th_Sniffer.join()
	return 1;
    }
	//th_Sniffer.join();
	return 0;
}
