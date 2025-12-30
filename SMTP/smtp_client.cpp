#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string>
#include <sstream>

const int BUFFER_SIZE = 4096;
const int SMTP_PORT = 2525;

class SMTPClient {
private:
    int sockfd;
    std::string server_host;
    std::string client_domain;
    
    bool sendCommand(const std::string& command) {
        std::string cmd = command + "\r\n";
        std::cout << "C: " << command << std::endl;
        
        ssize_t sent = write(sockfd, cmd.c_str(), cmd.length());
        if (sent < 0) {
            std::cerr << "Ошибка отправки команды" << std::endl;
            return false;
        }
        return true;
    }
    
    std::string receiveResponse() {
        std::string full_response = "";
        std::string current_line = "";
        char ch;
        bool response_complete = false;

        while (!response_complete) {
            current_line = "";
            while (true) {
                ssize_t bytes_read = read(sockfd, &ch, 1);
                if (bytes_read <= 0) {
                    std::cerr << "Соединение разорвано или ошибка чтения" << std::endl;
                    return "";
                }
                current_line += ch;
                if (current_line.length() >= 2 && 
                    current_line.substr(current_line.length() - 2) == "\r\n") {
                    break;
                }
            }

            std::cout << "S: " << current_line;
            full_response += current_line;

            if (current_line.length() >= 4) {
                if (isdigit(current_line[0]) && isdigit(current_line[1]) && isdigit(current_line[2])) {
                    char separator = current_line[3];
                    if (separator == ' ') {
                        response_complete = true;
                    } else if (separator == '-') {
                        response_complete = false;
                    } else {
                        response_complete = true;
                    }
                } else {
                     if (full_response.length() > current_line.length()) response_complete = true;
                }
            }
        }
        
        return full_response;
    }
    
    bool checkResponseCode(const std::string& response, const std::string& expected_code) {
        if (response.length() < 3) {
            return false;
        }
        return response.substr(0, 3) == expected_code;
    }
    
    bool resolveHost(const std::string& hostname, std::string& ip_address) {
        struct hostent* host = gethostbyname(hostname.c_str());
        if (host == nullptr) {
            std::cerr << "Не удалось разрешить имя хоста: " << hostname << std::endl;
            return false;
        }
        
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, host->h_addr_list[0], ip, INET_ADDRSTRLEN);
        ip_address = ip;
        std::cout << "Разрешён хост " << hostname << " -> " << ip_address << std::endl;
        return true;
    }

public:
    SMTPClient(const std::string& server, const std::string& domain = "localhost") 
        : sockfd(-1), server_host(server), client_domain(domain) {}
    
    ~SMTPClient() {
        if (sockfd != -1) {
            disconnect();
        }
    }
    
    bool connect() {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            std::cerr << "Ошибка создания сокета" << std::endl;
            return false;
        }
        
        std::string ip_address;
        if (!resolveHost(server_host, ip_address)) {
            close(sockfd);
            sockfd = -1;
            return false;
        }
        
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(SMTP_PORT);
        
        if (inet_pton(AF_INET, ip_address.c_str(), &server_addr.sin_addr) <= 0) {
            std::cerr << "Неверный IP-адрес" << std::endl;
            close(sockfd);
            sockfd = -1;
            return false;
        }
        
        std::cout << "Подключение к " << server_host << ":" << SMTP_PORT << "..." << std::endl;
        if (::connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "Ошибка подключения к серверу: " << strerror(errno) << std::endl;
            close(sockfd);
            sockfd = -1;
            return false;
        }
        
        std::cout << "Подключение установлено" << std::endl;
        
        std::string response = receiveResponse();
        if (response.empty() || !checkResponseCode(response, "220")) {
            std::cerr << "Неожиданный ответ при подключении" << std::endl;
            return false;
        }
        
        return true;
    }
    
    bool sendEmail(const std::string& from, const std::string& to, 
                   const std::string& subject, const std::string& body) {
        if (!sendCommand("EHLO " + client_domain)) {
            return false;
        }
        std::string response = receiveResponse();
        if (!checkResponseCode(response, "250")) {
            std::cerr << "Ошибка выполнения команды EHLO" << std::endl;
            return false;
        }
        
        if (!sendCommand("MAIL FROM:<" + from + ">")) {
            return false;
        }
        response = receiveResponse();
        if (!checkResponseCode(response, "250")) {
            std::cerr << "Ошибка выполнения команды MAIL FROM" << std::endl;
            return false;
        }
        
        if (!sendCommand("RCPT TO:<" + to + ">")) {
            return false;
        }
        response = receiveResponse();
        if (!checkResponseCode(response, "250")) {
            std::cerr << "Ошибка выполнения команды RCPT TO" << std::endl;
            return false;
        }
        
        if (!sendCommand("DATA")) {
            return false;
        }
        response = receiveResponse();
        if (!checkResponseCode(response, "354")) {
            std::cerr << "Ошибка выполнения команды DATA" << std::endl;
            return false;
        }
        
        std::stringstream email_content;
        email_content << "From: " << from << "\r\n";
        email_content << "To: " << to << "\r\n";
        email_content << "Subject: " << subject << "\r\n";
        email_content << "Content-Type: text/plain; charset=utf-8\r\n";
        email_content << "\r\n";
        email_content << body << "\r\n";
        email_content << ".\r\n";
        
        std::cout << "C: [Sending Email Body ending with .]" << std::endl;
        ssize_t sent = write(sockfd, email_content.str().c_str(), email_content.str().length());
        if (sent < 0) {
            std::cerr << "Ошибка отправки содержимого письма" << std::endl;
            return false;
        }
        
        response = receiveResponse();
        if (!checkResponseCode(response, "250")) {
            std::cerr << "Ошибка при отправке письма" << std::endl;
            return false;
        }
        
        std::cout << "Письмо успешно принято сервером!" << std::endl;
        return true;
    }
    
    void disconnect() {
        if (sockfd >= 0) {
            if (sendCommand("QUIT")) {
                std::string response = receiveResponse();
                if (checkResponseCode(response, "221")) {
                    std::cout << "Сессия корректно завершена (221)." << std::endl;
                } else {
                    std::cerr << "Предупреждение: Сервер вернул ошибку при выходе." << std::endl;
                }
            }
            
            close(sockfd);
            sockfd = -1;
            std::cout << "Соединение закрыто" << std::endl;
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc != 6) {
        std::cout << "Использование: " << argv[0] 
                  << " <smtp_server> <from_email> <to_email> <subject> <body>" << std::endl;
        return 1;
    }
    
    std::string smtp_server = argv[1];
    std::string from_email = argv[2];
    std::string to_email = argv[3];
    std::string subject = argv[4];
    std::string body = argv[5];
    
    char hostname[1024];
    gethostname(hostname, 1024);
    
    SMTPClient client(smtp_server, hostname);
    
    if (!client.connect()) {
        std::cerr << "Не удалось подключиться к SMTP-серверу" << std::endl;
        return 1;
    }
    
    if (!client.sendEmail(from_email, to_email, subject, body)) {
        std::cerr << "Не удалось отправить письмо" << std::endl;
        client.disconnect();
        return 1;
    }
    
    client.disconnect();
    
    return 0;
}