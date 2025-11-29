#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <netinet/in.h>
#include <mutex>

struct HttpRequest {
    std::string method;
    std::string path;
    std::string version;
};

struct HttpResponse {
    int statusCode;
    std::string contentType;
    std::string body;
    
    std::string toString() const;
};

class WebServer {
public:
    WebServer(int port, const std::string& publicDir);
    ~WebServer();

    void start();
    void stop();

    static HttpRequest parseRequest(const std::string& rawRequest);
    HttpResponse handleRequest(const HttpRequest& request);

private:
    int serverSocket;
    int port;
    std::string publicDir;
    bool isRunning;
    std::mutex logMutex;

    void handleClient(int clientSocket);
    std::string readFile(const std::string& path);
};

#endif // SERVER_HPP
