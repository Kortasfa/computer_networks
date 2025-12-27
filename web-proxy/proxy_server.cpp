#include "proxy_server.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <sys/socket.h>
#include <netdb.h>
#include <cstring>
#include <stdexcept>
#include <thread>

ProxyServer::ProxyServer(int port, const std::string& cacheDir)
    : port_(port), cacheDir_(cacheDir), isRunning_(false) {
    // Create cache directory if it doesn't exist
    struct stat info;
    if (stat(cacheDir_.c_str(), &info) != 0) {
        if (mkdir(cacheDir_.c_str(), 0755) != 0) {
            throw std::runtime_error("Failed to create cache directory: " + cacheDir_);
        }
    } else if (!(info.st_mode & S_IFDIR)) {
        throw std::runtime_error("Cache path exists but is not a directory: " + cacheDir_);
    }
}

ProxyServer::~ProxyServer() {
    stop();
}

void ProxyServer::start() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        throw std::runtime_error("Failed to create socket");
    }
    serverSocket_ = Socket(fd);
    
    int opt = 1;
    if (setsockopt(serverSocket_.get(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
    }
    
    sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port_);
    
    if (bind(serverSocket_.get(), (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        throw std::runtime_error("Failed to bind to port " + std::to_string(port_));
    }
    
    if (listen(serverSocket_.get(), 10) < 0) {
        throw std::runtime_error("Failed to listen on socket");
    }
    
    isRunning_ = true;
    log("Proxy server started on port " + std::to_string(port_));
    log("Cache directory: " + cacheDir_);
    
    while (isRunning_) {
        sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket_.get(), (struct sockaddr*)&clientAddr, &clientLen);
        
        if (clientSocket < 0) {
            if (isRunning_) {
                log("Failed to accept connection");
            }
            continue;
        }
        
        // Handle each client in a separate thread
        std::thread(&ProxyServer::handleClient, this, clientSocket).detach();
    }
}

void ProxyServer::stop() {
    isRunning_ = false;
    serverSocket_.close();
}

void ProxyServer::handleClient(int clientSocket) {
    Socket clientSocketWrapper(clientSocket);
    
    try {
        // Read request headers first
        std::string rawRequest;
        char buffer[8192] = {0};
        ssize_t totalRead = 0;
        
        // Read until we get headers (empty line)
        while (true) {
            ssize_t bytesRead = recv(clientSocketWrapper.get(), buffer + totalRead, 
                                     sizeof(buffer) - totalRead - 1, 0);
            if (bytesRead <= 0) {
                break;
            }
            totalRead += bytesRead;
            buffer[totalRead] = '\0';
            
            // Check if we have complete headers
            std::string temp(buffer, totalRead);
            if (temp.find("\r\n\r\n") != std::string::npos) {
                break;
            }
            
            // Prevent buffer overflow
            if (totalRead >= static_cast<ssize_t>(sizeof(buffer) - 1)) {
                break;
            }
        }
        
        if (totalRead <= 0) {
            return;
        }
        
        rawRequest.assign(buffer, totalRead);
        log("Received request from client (" + std::to_string(totalRead) + " bytes)");
        
        // Debug: print first line of request
        size_t firstLineEnd = rawRequest.find("\r\n");
        if (firstLineEnd != std::string::npos) {
            log("Request line: " + rawRequest.substr(0, firstLineEnd));
        }
        
        // Parse headers first to check if there's a body
        ParsedRequest request = parseRequest(rawRequest);
        
        // Read body if Content-Length is specified
        if (request.method == "POST" && request.headers.find("content-length") != request.headers.end()) {
            int contentLength = std::stoi(request.headers["content-length"]);
            if (contentLength > 0) {
                // Find where headers end
                size_t headerEnd = rawRequest.find("\r\n\r\n");
                if (headerEnd != std::string::npos) {
                    headerEnd += 4;
                    size_t bodyRead = rawRequest.length() - headerEnd;
                    
                    // Read remaining body if needed
                    if (bodyRead < static_cast<size_t>(contentLength)) {
                        std::vector<char> bodyBuffer(contentLength - bodyRead);
                        ssize_t bytesRead = recv(clientSocketWrapper.get(), bodyBuffer.data(), 
                                                 bodyBuffer.size(), 0);
                        if (bytesRead > 0) {
                            rawRequest.append(bodyBuffer.data(), bytesRead);
                        }
                    }
                }
            }
        }
        
        // Re-parse with full request including body
        request = parseRequest(rawRequest);
        
        if (request.method.empty()) {
            log("Failed to parse request - method is empty");
            log("Raw request (first 200 chars): " + rawRequest.substr(0, 200));
            sendResponse(clientSocketWrapper.get(), createErrorResponse(400, "Bad Request", "Failed to parse request"));
            return;
        }
        
        // Handle CONNECT method for HTTPS (basic support)
        if (request.method == "CONNECT") {
            log("CONNECT request received (HTTPS tunneling not fully supported)");
            sendResponse(clientSocketWrapper.get(), createErrorResponse(501, "Not Implemented", "HTTPS tunneling (CONNECT) is not supported"));
            return;
        }
        
        if (request.host.empty()) {
            log("Failed to parse request - host is empty");
            log("Raw request (first 200 chars): " + rawRequest.substr(0, 200));
            sendResponse(clientSocketWrapper.get(), createErrorResponse(400, "Bad Request", "Host header is missing"));
            return;
        }
        
        log("Request: " + request.method + " http://" + request.host + ":" + std::to_string(request.port) + request.path);
        
        updateStats(false, false);
        
        std::string cacheKey = generateCacheKey(request);
        std::string response;
        bool cacheHit = false;
        
        // Check cache for GET requests
        if (request.method == "GET" && isCached(cacheKey)) {
            CacheEntry entry = getCacheEntry(cacheKey);
            if (entry.statusCode == 200) {
                std::ifstream file(entry.filePath, std::ios::binary);
                if (file.is_open()) {
                    std::stringstream buffer;
                    buffer << file.rdbuf();
                    response = buffer.str();
                    cacheHit = true;
                    log("Cache HIT: " + request.host + request.path);
                    updateStats(true, false);
                }
            }
        }
        
        // Cache miss or POST request - fetch from server
        if (!cacheHit) {
            log("Cache MISS: " + request.host + request.path);
            updateStats(false, false);
            
            response = fetchFromServer(request);
            
            // Cache successful responses (not 404)
            if (!response.empty() && shouldCache(extractStatusCode(response))) {
                saveToCache(cacheKey, response, extractStatusCode(response));
            }
        }
        
        if (response.empty()) {
            log("Warning: Empty response generated");
            response = createErrorResponse(500, "Internal Server Error", "Proxy server error");
        }
        
        sendResponse(clientSocketWrapper.get(), response);
        log("Response sent to client");
        
    } catch (const std::exception& e) {
        log("Error handling client: " + std::string(e.what()));
        updateStats(false, true);
        try {
            sendResponse(clientSocketWrapper.get(), createErrorResponse(500, "Internal Server Error", "Proxy server error: " + std::string(e.what())));
        } catch (...) {
            // Ignore errors when sending error response
        }
    }
}

ParsedRequest ProxyServer::parseRequest(const std::string& rawRequest) {
    ParsedRequest request;
    std::istringstream iss(rawRequest);
    std::string line;
    
    // Parse request line
    if (!std::getline(iss, line)) {
        return request;
    }
    
    std::istringstream requestLine(line);
    requestLine >> request.method >> request.url >> request.version;
    
    // Remove trailing \r
    if (!request.version.empty() && request.version.back() == '\r') {
        request.version.pop_back();
    }
    
    // Parse headers
    while (std::getline(iss, line) && line != "\r" && !line.empty()) {
        if (line.back() == '\r') {
            line.pop_back();
        }
        
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 1);
            
            // Trim whitespace
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            // Convert to lowercase for consistency
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            request.headers[key] = value;
        }
    }
    
    // Extract host and port
    // First, try to extract from URL (for proxy requests, URL contains full URL)
    if (request.url.find("http://") == 0 || request.url.find("https://") == 0) {
        size_t protocolEnd = request.url.find("://") + 3;
        size_t pathStart = request.url.find('/', protocolEnd);
        if (pathStart == std::string::npos) {
            pathStart = request.url.length();
        }
        
        std::string hostPart = request.url.substr(protocolEnd, pathStart - protocolEnd);
        size_t colonPos = hostPart.find(':');
        if (colonPos != std::string::npos) {
            request.host = hostPart.substr(0, colonPos);
            try {
                request.port = std::stoi(hostPart.substr(colonPos + 1));
            } catch (...) {
                request.port = (request.url.find("https://") == 0) ? 443 : 80;
            }
        } else {
            request.host = hostPart;
            request.port = (request.url.find("https://") == 0) ? 443 : 80;
        }
        request.path = request.url.substr(pathStart);
    } else if (request.headers.find("host") != request.headers.end()) {
        // Extract from Host header
        std::string hostHeader = request.headers["host"];
        size_t colonPos = hostHeader.find(':');
        if (colonPos != std::string::npos) {
            request.host = hostHeader.substr(0, colonPos);
            try {
                request.port = std::stoi(hostHeader.substr(colonPos + 1));
            } catch (...) {
                request.port = 80;
            }
        } else {
            request.host = hostHeader;
            request.port = 80;
        }
        // Extract path from URL
        size_t pathStart = request.url.find('/');
        if (pathStart != std::string::npos) {
            request.path = request.url.substr(pathStart);
        } else {
            request.path = "/";
        }
    } else {
        // No host found - this is an error
        request.path = request.url;
    }
    
    // Ensure path is set
    if (request.path.empty()) {
        request.path = "/";
    }
    
    // Read body for POST requests
    if (request.method == "POST") {
        // Find where headers end
        size_t headerEnd = rawRequest.find("\r\n\r\n");
        if (headerEnd != std::string::npos) {
            headerEnd += 4; // Skip "\r\n\r\n"
            if (headerEnd < rawRequest.length()) {
                request.body = rawRequest.substr(headerEnd);
            }
        }
    }
    
    return request;
}

std::string ProxyServer::generateCacheKey(const ParsedRequest& request) {
    return request.host + ":" + std::to_string(request.port) + request.path;
}

std::string ProxyServer::getCacheFilePath(const std::string& cacheKey) {
    std::string safeKey = sanitizeFilename(cacheKey);
    return cacheDir_ + "/" + safeKey;
}

std::string ProxyServer::sanitizeFilename(const std::string& filename) const {
    std::string result;
    for (char c : filename) {
        if (std::isalnum(c) || c == '.' || c == '-' || c == '_' || c == '/') {
            if (c == '/') {
                result += '_';
            } else {
                result += c;
            }
        } else {
            result += '_';
        }
    }
    return result;
}

bool ProxyServer::isCached(const std::string& cacheKey) {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    std::string filePath = getCacheFilePath(cacheKey);
    std::ifstream file(filePath);
    return file.good();
}

CacheEntry ProxyServer::getCacheEntry(const std::string& cacheKey) {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    CacheEntry entry;
    entry.filePath = getCacheFilePath(cacheKey);
    
    struct stat fileInfo;
    if (stat(entry.filePath.c_str(), &fileInfo) == 0) {
        entry.timestamp = fileInfo.st_mtime;
    }
    
    // Try to read status code from metadata file
    std::string metaPath = entry.filePath + ".meta";
    std::ifstream metaFile(metaPath);
    if (metaFile.is_open()) {
        std::string line;
        if (std::getline(metaFile, line)) {
            entry.statusCode = std::stoi(line);
        }
        if (std::getline(metaFile, line)) {
            entry.lastModified = line;
        }
    } else {
        entry.statusCode = 200; // Assume 200 if no metadata
    }
    
    return entry;
}

void ProxyServer::saveToCache(const std::string& cacheKey, const std::string& response, int statusCode) {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    
    std::string filePath = getCacheFilePath(cacheKey);
    
    // Create directory structure if needed
    size_t lastSlash = filePath.find_last_of('/');
    if (lastSlash != std::string::npos) {
        std::string dir = filePath.substr(0, lastSlash);
        struct stat info;
        if (stat(dir.c_str(), &info) != 0) {
            // Create directory recursively would be better, but for simplicity...
            // For now, just use flat structure
        }
    }
    
    // Save response to file
    std::ofstream file(filePath, std::ios::binary);
    if (file.is_open()) {
        file.write(response.c_str(), response.size());
        file.close();
        
        // Save metadata
        std::string metaPath = filePath + ".meta";
        std::ofstream metaFile(metaPath);
        if (metaFile.is_open()) {
            metaFile << statusCode << "\n";
            metaFile << std::time(nullptr) << "\n";
            metaFile.close();
        }
    }
}

std::string ProxyServer::fetchFromServer(const ParsedRequest& request) {
    Socket serverSocket;
    
    // Resolve hostname
    struct hostent* hostEntry = gethostbyname(request.host.c_str());
    if (!hostEntry) {
        log("Failed to resolve hostname: " + request.host);
        return createErrorResponse(502, "Bad Gateway", "Failed to resolve hostname");
    }
    
    // Create socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        log("Failed to create socket for server connection");
        return createErrorResponse(502, "Bad Gateway", "Failed to create socket");
    }
    serverSocket = Socket(fd);
    
    // Connect to server
    sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(request.port);
    std::memcpy(&serverAddr.sin_addr, hostEntry->h_addr_list[0], hostEntry->h_length);
    
    if (connect(serverSocket.get(), (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        log("Failed to connect to server: " + request.host + ":" + std::to_string(request.port));
        return createErrorResponse(502, "Bad Gateway", "Failed to connect to server");
    }
    
    // Build request to forward
    std::ostringstream requestStream;
    requestStream << request.method << " " << request.path << " " << request.version << "\r\n";
    
    // Forward headers, but modify Host header
    for (const auto& header : request.headers) {
        if (header.first != "host" && header.first != "connection" && 
            header.first != "proxy-connection") {
            requestStream << header.first << ": " << header.second << "\r\n";
        }
    }
    requestStream << "Host: " << request.host;
    if (request.port != 80) {
        requestStream << ":" << request.port;
    }
    requestStream << "\r\n";
    requestStream << "Connection: close\r\n";
    requestStream << "\r\n";
    
    // Add body for POST requests
    if (request.method == "POST" && !request.body.empty()) {
        requestStream << request.body;
    }
    
    std::string requestStr = requestStream.str();
    
    // Send request
    if (send(serverSocket.get(), requestStr.c_str(), requestStr.size(), 0) < 0) {
        log("Failed to send request to server");
        return createErrorResponse(502, "Bad Gateway", "Failed to send request");
    }
    
    // Read response
    std::string response = readFullResponse(serverSocket);
    
    if (response.empty()) {
        return createErrorResponse(502, "Bad Gateway", "Empty response from server");
    }
    
    return response;
}

std::string ProxyServer::readFullResponse(Socket& socket) {
    std::string response;
    char buffer[8192];
    
    while (true) {
        ssize_t bytesRead = recv(socket.get(), buffer, sizeof(buffer), 0);
        if (bytesRead <= 0) {
            break;
        }
        response.append(buffer, bytesRead);
        
        // Check if we've received the full response
        // For HTTP/1.0 or Connection: close, we read until EOF
        // For HTTP/1.1 with Content-Length, we could parse it, but for simplicity
        // we'll just read until connection closes
    }
    
    return response;
}

void ProxyServer::sendResponse(int clientSocket, const std::string& response) {
    if (response.empty()) {
        return;
    }
    
    ssize_t sent = 0;
    size_t total = response.size();
    const char* data = response.c_str();
    
    while (sent < static_cast<ssize_t>(total)) {
        ssize_t bytesSent = send(clientSocket, data + sent, total - sent, 0);
        if (bytesSent < 0) {
            log("Failed to send response to client");
            break;
        }
        sent += bytesSent;
    }
}

int ProxyServer::extractStatusCode(const std::string& response) {
    // Extract status code from HTTP response
    // Format: "HTTP/1.1 200 OK\r\n..."
    size_t space1 = response.find(' ');
    if (space1 == std::string::npos) {
        return 0;
    }
    size_t space2 = response.find(' ', space1 + 1);
    if (space2 == std::string::npos) {
        return 0;
    }
    
    std::string statusStr = response.substr(space1 + 1, space2 - space1 - 1);
    try {
        return std::stoi(statusStr);
    } catch (...) {
        return 0;
    }
}

std::string ProxyServer::createErrorResponse(int statusCode, const std::string& statusText, 
                                             const std::string& message) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n";
    oss << "Content-Type: text/plain\r\n";
    oss << "Content-Length: " << message.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << message;
    return oss.str();
}

bool ProxyServer::shouldCache(int statusCode) const {
    // Don't cache 404 errors and other error responses
    return statusCode == 200;
}

void ProxyServer::log(const std::string& message) const {
    std::lock_guard<std::mutex> lock(logMutex_);
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::cout << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "] " 
              << message << std::endl;
}

void ProxyServer::updateStats(bool cacheHit, bool error) {
    std::lock_guard<std::mutex> lock(statsMutex_);
    stats_.totalRequests++;
    if (cacheHit) {
        stats_.cacheHits++;
    } else {
        stats_.cacheMisses++;
    }
    if (error) {
        stats_.errors++;
    }
}

ProxyServer::Stats ProxyServer::getStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

