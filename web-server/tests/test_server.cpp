#include "../src/server.hpp"
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

void test_parseRequest() {
    std::cout << "Running test_parseRequest..." << std::endl;
    std::string raw = "GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n";
    HttpRequest req = WebServer::parseRequest(raw);
    
    assert(req.method == "GET");
    assert(req.path == "/index.html");
    assert(req.version == "HTTP/1.1");
    std::cout << "test_parseRequest PASSED" << std::endl;
}

void test_handleRequest_NotFound() {
    std::cout << "Running test_handleRequest_NotFound..." << std::endl;
    WebServer server(8080, "./public");
    HttpRequest req;
    req.method = "GET";
    req.path = "/nonexistent.html";
    req.version = "HTTP/1.1";

    HttpResponse res = server.handleRequest(req);
    assert(res.statusCode == 404);
    assert(res.body == "File Not Found");
    std::cout << "test_handleRequest_NotFound PASSED" << std::endl;
}

void test_handleRequest_Success() {
    std::cout << "Running test_handleRequest_Success..." << std::endl;
    WebServer server(8080, "./public");
    HttpRequest req;
    req.method = "GET";
    req.path = "/index.html";
    req.version = "HTTP/1.1";

    HttpResponse res = server.handleRequest(req);
    assert(res.statusCode == 200);
    assert(res.body.find("Hello, World!") != std::string::npos);
    std::cout << "test_handleRequest_Success PASSED" << std::endl;
}

// Integration test
void test_integration() {
    std::cout << "Running test_integration..." << std::endl;
    
    // Start server in a separate thread
    std::thread serverThread([]() {
        WebServer server(8888, "./public");
        // We need a way to stop the server loop for testing, 
        // but for this simple test we can just let it run and kill the process or detach.
        // However, since accept() blocks, it's hard to stop cleanly without non-blocking sockets or select().
        // For this simple assignment, we'll just run it and detach, knowing the OS cleans up when main exits.
        // OR we can just test the logic units above and skip full integration if threading is complex.
        // But let's try a simple client connection.
        try {
            server.start();
        } catch (...) {}
    });
    serverThread.detach();

    // Give server time to start
    std::this_thread::sleep_for(std::chrono::seconds(1));

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);

    sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8888);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection Failed" << std::endl;
        assert(false);
    }

    std::string request = "GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n";
    send(sock, request.c_str(), request.length(), 0);

    char buffer[1024] = {0};
    read(sock, buffer, 1024);
    std::string response(buffer);

    assert(response.find("200 OK") != std::string::npos);
    assert(response.find("Hello, World!") != std::string::npos);

    close(sock);
    std::cout << "test_integration PASSED" << std::endl;
    
    // Note: The server thread is still running, but the test program will exit now.
}

int main() {
    test_parseRequest();
    test_handleRequest_NotFound();
    test_handleRequest_Success();
    test_integration();
    
    std::cout << "All tests passed!" << std::endl;
    return 0;
}
