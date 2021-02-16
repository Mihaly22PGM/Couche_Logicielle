#include <iostream>
#include <string>
#include <functional>
#include <tins/tins.h>



int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: " << *argv << " <interface>" << std:::endl;
        return 1;
    }
    try {
        tcp_connection_closer closer;
        closer.run(argv[1]);
    }
    catch (exception& ex) {
        std::cout << "[-] Error: " << ex.what() << std::endl;
        return 1;
    }
}
