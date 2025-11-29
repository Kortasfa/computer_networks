#include "server.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>
#include <thread>

std::string HttpResponse::toString() const {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << statusCode << " ";
    if (statusCode == 200) oss << "OK";
    else if (statusCode == 404) oss << "Not Found";
    else oss << "Unknown";
    
    oss << "\r\n";
    oss << "Content-Type: " << contentType << "\r\n";
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "\r\n";
    oss << body;
    return oss.str();
}

WebServer::WebServer(int port, const std::string& publicDir) 
    : serverSocket(-1), port(port), publicDir(publicDir), isRunning(false) {
}

WebServer::~WebServer() {
    stop();
}

void WebServer::start() {
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        throw std::runtime_error("Failed to bind to port " + std::to_string(port));
    }

    if (listen(serverSocket, 10) < 0) {
        throw std::runtime_error("Failed to listen on socket");
    }

    isRunning = true;
    std::cout << "Server started on port " << port << " serving " << publicDir << std::endl;
    while (isRunning) {
        sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);

        if (clientSocket < 0) {
            if (isRunning) {
                std::lock_guard<std::mutex> lock(logMutex);
                std::cerr << "Failed to accept connection" << std::endl;
            }
            continue;
        }

        std::thread(&WebServer::handleClient, this, clientSocket).detach();
    }
}

void WebServer::stop() {
    isRunning = false;
    if (serverSocket >= 0) {
        close(serverSocket);
        serverSocket = -1;
    }
}

void WebServer::handleClient(int clientSocket) {
    char buffer[4096] = {0};
    ssize_t bytesRead = read(clientSocket, buffer, sizeof(buffer) - 1);
    
    if (bytesRead > 0) {
        std::string rawRequest(buffer);
        
        {
            std::lock_guard<std::mutex> lock(logMutex);
            std::cout << "Received request:\n" << rawRequest.substr(0, rawRequest.find("\r\n")) << std::endl;
        }

        HttpRequest request = parseRequest(rawRequest);
        HttpResponse response = handleRequest(request);
        
        std::string responseStr = response.toString();
        write(clientSocket, responseStr.c_str(), responseStr.size());
    }

    close(clientSocket);
}

HttpRequest WebServer::parseRequest(const std::string& rawRequest) {
    HttpRequest request;
    std::istringstream iss(rawRequest);
    iss >> request.method >> request.path >> request.version;
    return request;
}

HttpResponse WebServer::handleRequest(const HttpRequest& request) {
    HttpResponse response;

    if (request.method != "GET") {
        response.statusCode = 405; // Method Not Allowed (simplified handling as per task reqs usually only ask for GET)
        // But task says "Support GET", implies others might be ignored or error. 
        // Task 3.3.2 says "Support method GET".
        // Let's stick to the task requirements for 404 and 200.
        // If not GET, we can treat as error or just try to serve. 
        // Let's assume we only handle GET.
    }

    std::string filePath = publicDir + request.path;
    
    // Default to index.html if path is /
    if (request.path == "/") {
        filePath += "index.html";
    }

    // Simple security check to prevent directory traversal
    if (filePath.find("..") != std::string::npos) {
         response.statusCode = 404;
         response.contentType = "text/plain";
         response.body = "File Not Found";
         return response;
    }

    std::string content = readFile(filePath);

    if (!content.empty()) {
        response.statusCode = 200;
        response.contentType = "text/html";
        response.body = content;
    } else {
        response.statusCode = 404;
        response.contentType = "text/plain";
        response.body = "File Not Found";
    }

    return response;
}

std::string WebServer::readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}
