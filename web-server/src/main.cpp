#include "server.hpp"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    int port = 8080;
    std::string publicDir = "./public";

    if (argc > 1) {
        port = std::stoi(argv[1]);
    }
    if (argc > 2) {
        publicDir = argv[2];
    }

    try {
        WebServer server(port, publicDir);
        server.start();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
