#include "proxy_server.hpp"
#include <iostream>
#include <csignal>
#include <cstdlib>

ProxyServer* g_proxyServer = nullptr;

void signalHandler(int signal) {
    if (g_proxyServer && signal == SIGINT) {
        std::cout << "\nShutting down proxy server..." << std::endl;
        g_proxyServer->stop();
        exit(0);
    }
}

int main(int argc, char* argv[]) {
    int port = 8080;
    std::string cacheDir = "./cache";
    
    // Parse command line arguments
    if (argc > 1) {
        port = std::stoi(argv[1]);
    }
    if (argc > 2) {
        cacheDir = argv[2];
    }
    
    try {
        ProxyServer server(port, cacheDir);
        g_proxyServer = &server;
        
        // Set up signal handler for graceful shutdown
        signal(SIGINT, signalHandler);
        
        std::cout << "Starting HTTP Proxy Server..." << std::endl;
        std::cout << "Port: " << port << std::endl;
        std::cout << "Cache directory: " << cacheDir << std::endl;
        std::cout << "Press Ctrl+C to stop the server" << std::endl;
        
        server.start();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

