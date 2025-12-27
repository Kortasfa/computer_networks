#ifndef PROXY_SERVER_HPP
#define PROXY_SERVER_HPP

#include <string>
#include <netinet/in.h>
#include <mutex>
#include <map>
#include <memory>
#include <fstream>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctime>
#include <iomanip>
#include <algorithm>
#include <cctype>

// RAII wrapper for socket
class Socket {
public:
    Socket(int fd = -1) : fd_(fd) {}
    ~Socket() { close(); }
    
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    
    Socket(Socket&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }
    
    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) {
            close();
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }
    
    int get() const { return fd_; }
    bool valid() const { return fd_ >= 0; }
    void close() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }
    
    int release() {
        int temp = fd_;
        fd_ = -1;
        return temp;
    }
    
private:
    int fd_;
};

struct ParsedRequest {
    std::string method;
    std::string url;
    std::string host;
    std::string path;
    int port;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
    
    ParsedRequest() : port(80) {}
};

struct CacheEntry {
    std::string filePath;
    std::string lastModified;
    time_t timestamp;
    int statusCode;
    
    CacheEntry() : timestamp(0), statusCode(0) {}
};

class ProxyServer {
public:
    ProxyServer(int port, const std::string& cacheDir = "./cache");
    ~ProxyServer();
    
    void start();
    void stop();
    
    // Statistics
    struct Stats {
        size_t totalRequests;
        size_t cacheHits;
        size_t cacheMisses;
        size_t errors;
        
        Stats() : totalRequests(0), cacheHits(0), cacheMisses(0), errors(0) {}
    };
    
    Stats getStats() const;

private:
    Socket serverSocket_;
    int port_;
    std::string cacheDir_;
    bool isRunning_;
    mutable std::mutex logMutex_;
    mutable std::mutex cacheMutex_;
    mutable std::mutex statsMutex_;
    mutable Stats stats_;
    
    void handleClient(int clientSocket);
    ParsedRequest parseRequest(const std::string& rawRequest);
    std::string generateCacheKey(const ParsedRequest& request);
    std::string getCacheFilePath(const std::string& cacheKey);
    bool isCached(const std::string& cacheKey);
    CacheEntry getCacheEntry(const std::string& cacheKey);
    void saveToCache(const std::string& cacheKey, const std::string& response, int statusCode);
    std::string fetchFromServer(const ParsedRequest& request);
    void sendResponse(int clientSocket, const std::string& response);
    std::string readFullResponse(Socket& socket);
    int extractStatusCode(const std::string& response);
    std::string createErrorResponse(int statusCode, const std::string& statusText, 
                                    const std::string& message);
    void log(const std::string& message) const;
    void updateStats(bool cacheHit, bool error = false);
    bool shouldCache(int statusCode) const;
    std::string sanitizeFilename(const std::string& filename) const;
};

#endif // PROXY_SERVER_HPP

