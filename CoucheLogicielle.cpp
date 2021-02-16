#include "Snif.h"

#include "iostream"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: " << *argv << " <interface>" << std:::endl;
        return 1;
    }
    try {
        Snif snif;
        snif.run(argv[1]);
    }
    catch (exception& ex) {
        std::cout << "[-] Error: " << ex.what() << std::endl;
        return 1;
    }
}
