#include "dns_resolver.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <ctime>
#include <algorithm>
#include <stdexcept>

// Корневые DNS серверы
const std::vector<std::string> DNSResolver::ROOT_SERVERS = {
    "198.41.0.4",      // a.root-servers.net
    "199.9.14.201",    // b.root-servers.net
    "192.33.4.12",     // c.root-servers.net
    "199.7.91.13",     // d.root-servers.net
    "192.203.230.10",  // e.root-servers.net
    "192.5.5.241",     // f.root-servers.net
    "192.112.36.4",    // g.root-servers.net
    "198.97.190.53",   // h.root-servers.net
    "192.36.148.17",   // i.root-servers.net
    "192.58.128.30",   // j.root-servers.net
    "193.0.14.129",    // k.root-servers.net
    "199.7.83.42",     // l.root-servers.net
    "202.12.27.33"     // m.root-servers.net
};

// ==================== UDPSocket Implementation ====================

UDPSocket::UDPSocket() : sockfd_(-1) {
    sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_ < 0) {
        std::cerr << "Error creating UDP socket: " << strerror(errno) << std::endl;
        return;
    }
    
    // Установка опций сокета для лучшей работы
    int opt = 1;
    setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
}

UDPSocket::~UDPSocket() {
    if (sockfd_ >= 0) {
        close(sockfd_);
    }
}

UDPSocket::UDPSocket(UDPSocket&& other) noexcept : sockfd_(other.sockfd_) {
    other.sockfd_ = -1;
}

UDPSocket& UDPSocket::operator=(UDPSocket&& other) noexcept {
    if (this != &other) {
        if (sockfd_ >= 0) {
            close(sockfd_);
        }
        sockfd_ = other.sockfd_;
        other.sockfd_ = -1;
    }
    return *this;
}

bool UDPSocket::sendTo(const std::string& server_ip, uint16_t port, const void* data, size_t len) {
    if (!isValid()) {
        return false;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "Invalid IP address: " << server_ip << std::endl;
        return false;
    }
    
    ssize_t sent = sendto(sockfd_, data, len, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
    return sent == static_cast<ssize_t>(len);
}

ssize_t UDPSocket::recvFrom(void* buffer, size_t len, std::string& from_ip, uint16_t& from_port, int timeout_sec) {
    if (!isValid()) {
        return -1;
    }
    
    // Установка таймаута через setsockopt для более надежной работы
    struct timeval timeout;
    timeout.tv_sec = timeout_sec;
    timeout.tv_usec = 0;
    if (setsockopt(sockfd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        // Если не удалось установить через setsockopt, используем select
    }
    
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(sockfd_, &read_fds);
    
    int select_result = select(sockfd_ + 1, &read_fds, nullptr, nullptr, &timeout);
    if (select_result < 0) {
        // Ошибка select
        return -1;
    }
    if (select_result == 0) {
        // Таймаут
        return -1;
    }
    
    if (!FD_ISSET(sockfd_, &read_fds)) {
        return -1;
    }
    
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);
    
    ssize_t received = recvfrom(sockfd_, buffer, len, 0, (struct sockaddr*)&from_addr, &from_len);
    
    if (received > 0) {
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &from_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
        from_ip = ip_str;
        from_port = ntohs(from_addr.sin_port);
    }
    
    return received;
}

// ==================== TCPSocket Implementation ====================

TCPSocket::TCPSocket() : sockfd_(-1) {
    sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_ < 0) {
        std::cerr << "Error creating TCP socket: " << strerror(errno) << std::endl;
    }
}

TCPSocket::~TCPSocket() {
    if (sockfd_ >= 0) {
        close(sockfd_);
    }
}

TCPSocket::TCPSocket(TCPSocket&& other) noexcept : sockfd_(other.sockfd_) {
    other.sockfd_ = -1;
}

TCPSocket& TCPSocket::operator=(TCPSocket&& other) noexcept {
    if (this != &other) {
        if (sockfd_ >= 0) {
            close(sockfd_);
        }
        sockfd_ = other.sockfd_;
        other.sockfd_ = -1;
    }
    return *this;
}

bool TCPSocket::connectTo(const std::string& server_ip, uint16_t port) {
    if (!isValid()) {
        return false;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "Invalid IP address: " << server_ip << std::endl;
        return false;
    }
    
    if (connect(sockfd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        return false;
    }
    
    return true;
}

ssize_t TCPSocket::send(const void* data, size_t len) {
    if (!isValid()) {
        return -1;
    }
    return ::send(sockfd_, data, len, 0);
}

ssize_t TCPSocket::recv(void* buffer, size_t len, int timeout_sec) {
    if (!isValid()) {
        return -1;
    }
    
    // Установка таймаута через setsockopt
    struct timeval timeout;
    timeout.tv_sec = timeout_sec;
    timeout.tv_usec = 0;
    if (setsockopt(sockfd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        // Если не удалось установить через setsockopt, используем select
    }
    
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(sockfd_, &read_fds);
    
    int select_result = select(sockfd_ + 1, &read_fds, nullptr, nullptr, &timeout);
    if (select_result < 0) {
        // Ошибка select
        return -1;
    }
    if (select_result == 0) {
        // Таймаут
        return -1;
    }
    
    if (!FD_ISSET(sockfd_, &read_fds)) {
        return -1;
    }
    
    return ::recv(sockfd_, buffer, len, 0);
}

// ==================== DNSResolver Implementation ====================

DNSResolver::DNSResolver(bool debug_mode) : debug_mode_(debug_mode) {
}

bool DNSResolver::isValidDomainName(const std::string& domain) {
    if (domain.empty() || domain.length() > 253) {
        return false;
    }
    
    // Проверка на допустимые символы
    for (char c : domain) {
        if (!((c >= 'a' && c <= 'z') || 
              (c >= 'A' && c <= 'Z') || 
              (c >= '0' && c <= '9') || 
              c == '-' || c == '.')) {
            return false;
        }
    }
    
    return true;
}

std::vector<uint8_t> DNSResolver::createDNSQuery(uint16_t id, const std::string& domain, DNSRecordType type) {
    std::vector<uint8_t> packet;
    
    // Заголовок
    DNSHeader header;
    header.id = htons(id);
    // Флаги: QR=0, Opcode=0, AA=0, TC=0, RD=0 (итеративный запрос!), RA=0, Z=0, RCODE=0
    // В бинарном виде: 0000 0000 0000 0000 = 0x0000
    header.flags = htons(0x0000); // Итеративный запрос без рекурсии
    header.qdcount = htons(1);
    header.ancount = 0;
    header.nscount = 0;
    header.arcount = 0;
    
    packet.insert(packet.end(), reinterpret_cast<uint8_t*>(&header), 
                  reinterpret_cast<uint8_t*>(&header) + sizeof(header));
    
    // QNAME - кодирование доменного имени
    std::istringstream iss(domain);
    std::string label;
    while (std::getline(iss, label, '.')) {
        if (label.length() > 63) {
            throw std::runtime_error("Label too long");
        }
        packet.push_back(static_cast<uint8_t>(label.length()));
        packet.insert(packet.end(), label.begin(), label.end());
    }
    packet.push_back(0); // Конец имени
    
    // Question
    DNSQuestion question;
    question.qtype = htons(static_cast<uint16_t>(type));
    question.qclass = htons(static_cast<uint16_t>(DNSClass::IN));
    
    packet.insert(packet.end(), reinterpret_cast<uint8_t*>(&question), 
                  reinterpret_cast<uint8_t*>(&question) + sizeof(question));
    
    return packet;
}

std::string DNSResolver::parseDNSName(const std::vector<uint8_t>& packet, size_t& offset) {
    std::string name;
    size_t start_offset = offset;
    bool jumped = false;
    size_t jump_count = 0;
    const size_t MAX_JUMPS = 10; // Защита от бесконечных циклов
    
    while (offset < packet.size() && jump_count < MAX_JUMPS) {
        uint8_t length = packet[offset];
        
        // Проверка на компрессию (первые 2 бита = 11)
        if ((length & 0xC0) == 0xC0) {
            if (offset + 1 >= packet.size()) {
                break;
            }
            
            // Извлечение смещения
            uint16_t pointer = ((length & 0x3F) << 8) | packet[offset + 1];
            offset += 2;
            
            if (!jumped) {
                start_offset = offset;
                jumped = true;
            }
            
            offset = pointer;
            jump_count++;
            continue;
        }
        
        // Обычная метка
        if (length == 0) {
            offset++;
            break;
        }
        
        if (offset + length >= packet.size()) {
            break;
        }
        
        if (!name.empty()) {
            name += ".";
        }
        
        name.append(reinterpret_cast<const char*>(&packet[offset + 1]), length);
        offset += length + 1;
    }
    
    if (!jumped) {
        start_offset = offset;
    }
    offset = start_offset;
    
    return name;
}

bool DNSResolver::parseResourceRecord(const std::vector<uint8_t>& packet, size_t& offset, DNSResourceRecord& rr) {
    if (offset >= packet.size()) {
        return false;
    }
    
    // Имя
    rr.name = parseDNSName(packet, offset);
    
    if (offset + 10 > packet.size()) {
        return false;
    }
    
    // Тип, класс, TTL, длина данных
    rr.type = static_cast<DNSRecordType>(ntohs(*reinterpret_cast<const uint16_t*>(&packet[offset])));
    offset += 2;
    
    rr.class_ = static_cast<DNSClass>(ntohs(*reinterpret_cast<const uint16_t*>(&packet[offset])));
    offset += 2;
    
    rr.ttl = ntohl(*reinterpret_cast<const uint32_t*>(&packet[offset]));
    offset += 4;
    
    rr.data_length = ntohs(*reinterpret_cast<const uint16_t*>(&packet[offset]));
    offset += 2;
    
    if (offset + rr.data_length > packet.size()) {
        return false;
    }
    
    // Извлечение данных
    rr.data.assign(&packet[offset], &packet[offset] + rr.data_length);
    offset += rr.data_length;
    
    // Парсинг данных в зависимости от типа
    if (rr.type == DNSRecordType::A && rr.data_length == 4) {
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, rr.data.data(), ip, INET_ADDRSTRLEN);
        rr.ipv4_address = ip;
    } else if (rr.type == DNSRecordType::AAAA && rr.data_length == 16) {
        rr.ipv6_address = ipv6ToString(rr.data.data());
    } else if (rr.type == DNSRecordType::NS || rr.type == DNSRecordType::CNAME) {
        size_t name_offset = offset - rr.data_length;
        rr.ns_name = parseDNSName(packet, name_offset);
        if (rr.type == DNSRecordType::CNAME) {
            rr.cname = rr.ns_name;
        }
    }
    
    return true;
}

bool DNSResolver::parseDNSResponse(const std::vector<uint8_t>& response, 
                                   DNSRecordType /* query_type */,
                                   std::vector<DNSResourceRecord>& answers,
                                   std::vector<DNSResourceRecord>& authority,
                                   std::vector<DNSResourceRecord>& additional) {
    if (response.size() < sizeof(DNSHeader)) {
        return false;
    }
    
    const DNSHeader* header = reinterpret_cast<const DNSHeader*>(response.data());
    uint16_t flags = ntohs(header->flags);
    
    // Проверка на ошибки
    uint8_t rcode = flags & 0x0F;
    if (rcode == 3) { // NXDOMAIN
        debugLog("Domain not found (NXDOMAIN)");
        return false;
    } else if (rcode != 0) {
        debugLog("DNS error code: " + std::to_string(rcode));
        return false;
    }
    
    size_t offset = sizeof(DNSHeader);
    
    // Пропуск вопросов
    uint16_t qdcount = ntohs(header->qdcount);
    for (uint16_t i = 0; i < qdcount && offset < response.size(); i++) {
        parseDNSName(response, offset);
        if (offset + 4 > response.size()) {
            return false;
        }
        offset += 4; // qtype + qclass
    }
    
    // Парсинг ответов
    answers.clear();
    uint16_t ancount = ntohs(header->ancount);
    for (uint16_t i = 0; i < ancount && offset < response.size(); i++) {
        DNSResourceRecord rr;
        if (parseResourceRecord(response, offset, rr)) {
            answers.push_back(rr);
        } else {
            break;
        }
    }
    
    // Парсинг Authority секции
    authority.clear();
    uint16_t nscount = ntohs(header->nscount);
    for (uint16_t i = 0; i < nscount && offset < response.size(); i++) {
        DNSResourceRecord rr;
        if (parseResourceRecord(response, offset, rr)) {
            authority.push_back(rr);
        } else {
            break;
        }
    }
    
    // Парсинг Additional секции
    additional.clear();
    uint16_t arcount = ntohs(header->arcount);
    for (uint16_t i = 0; i < arcount && offset < response.size(); i++) {
        DNSResourceRecord rr;
        if (parseResourceRecord(response, offset, rr)) {
            additional.push_back(rr);
        } else {
            break;
        }
    }
    
    return true;
}

bool DNSResolver::queryUDP(const std::string& server_ip, const std::vector<uint8_t>& query, 
                          std::vector<uint8_t>& response, bool& truncated, uint16_t expected_id) {
    UDPSocket socket;
    if (!socket.isValid()) {
        debugLog("Failed to create UDP socket: " + std::string(strerror(errno)));
        return false;
    }
    
    debugLog("Querying " + server_ip + " via UDP");
    
    if (!socket.sendTo(server_ip, 53, query.data(), query.size())) {
        debugLog("Failed to send UDP query: " + std::string(strerror(errno)));
        return false;
    }
    
    uint8_t buffer[512];
    std::string from_ip;
    uint16_t from_port;
    // Короткий таймаут для быстрого переключения между серверами
    ssize_t received = socket.recvFrom(buffer, sizeof(buffer), from_ip, from_port, 3);
    
    if (received < static_cast<ssize_t>(sizeof(DNSHeader))) {
        if (received < 0) {
            debugLog("No response or timeout from " + server_ip);
        } else {
            debugLog("Response too short from " + server_ip);
        }
        return false;
    }
    
    // Проверяем, что ответ пришел от правильного сервера
    if (from_ip != server_ip) {
        debugLog("Response from different IP: " + from_ip + " (expected " + server_ip + ")");
        // Все равно принимаем ответ, так как некоторые серверы могут использовать другой IP
    }
    
    response.assign(buffer, buffer + received);
    
    // Проверка заголовка
    if (response.size() < sizeof(DNSHeader)) {
        debugLog("Response too short");
        return false;
    }
    
    const DNSHeader* header = reinterpret_cast<const DNSHeader*>(response.data());
    
    // Проверка ID запроса (защита от подмены ответов)
    uint16_t response_id = ntohs(header->id);
    if (response_id != expected_id) {
        debugLog("Response ID mismatch: expected " + std::to_string(expected_id) + 
                 ", got " + std::to_string(response_id));
        return false;
    }
    
    // Проверка на усеченный пакет
    uint16_t flags = ntohs(header->flags);
    truncated = (flags & 0x0200) != 0; // TC (Truncated) флаг
    
    if (truncated) {
        debugLog("Response truncated, will retry via TCP");
    }
    
    debugLog("Received " + std::to_string(received) + " bytes from " + from_ip);
    return true;
}

bool DNSResolver::queryTCP(const std::string& server_ip, const std::vector<uint8_t>& query, 
                          std::vector<uint8_t>& response, uint16_t expected_id) {
    TCPSocket socket;
    if (!socket.isValid()) {
        return false;
    }
    
    debugLog("Querying " + server_ip + " via TCP");
    
    if (!socket.connectTo(server_ip, 53)) {
        debugLog("Failed to connect via TCP");
        return false;
    }
    
    // Для TCP нужно добавить длину пакета в начале (2 байта)
    uint16_t query_length = htons(static_cast<uint16_t>(query.size()));
    if (socket.send(&query_length, 2) != 2) {
        debugLog("Failed to send TCP length prefix");
        return false;
    }
    
    if (socket.send(query.data(), query.size()) != static_cast<ssize_t>(query.size())) {
        debugLog("Failed to send TCP query");
        return false;
    }
    
    // Чтение длины ответа
    uint16_t response_length;
    if (socket.recv(&response_length, 2, 5) != 2) {
        debugLog("Failed to receive TCP length prefix");
        return false;
    }
    
    response_length = ntohs(response_length);
    if (response_length > 65535) {
        debugLog("Invalid response length");
        return false;
    }
    
    // Чтение ответа
    std::vector<uint8_t> buffer(response_length);
    ssize_t received = socket.recv(buffer.data(), response_length, 5);
    
    if (received != static_cast<ssize_t>(response_length)) {
        debugLog("Incomplete TCP response");
        return false;
    }
    
    response = std::move(buffer);
    
    // Проверка заголовка и ID
    if (response.size() < sizeof(DNSHeader)) {
        debugLog("TCP response too short");
        return false;
    }
    
    const DNSHeader* header = reinterpret_cast<const DNSHeader*>(response.data());
    uint16_t response_id = ntohs(header->id);
    if (response_id != expected_id) {
        debugLog("TCP response ID mismatch: expected " + std::to_string(expected_id) + 
                 ", got " + std::to_string(response_id));
        return false;
    }
    
    return true;
}

bool DNSResolver::iterativeResolve(const std::string& domain, DNSRecordType type, 
                                   const std::vector<std::string>& name_servers,
                                   std::vector<std::string>& results) {
    const int MAX_ITERATIONS = 20; // Защита от бесконечных циклов
    const int MAX_SERVER_TRIES = 2; // Максимум попыток для каждого уровня серверов (уменьшено для быстрого выхода)
    int iterations = 0;
    int server_tries = 0;
    int total_failures = 0;
    const int MAX_TOTAL_FAILURES = 5; // Общее ограничение на количество неудач
    
    std::vector<std::string> current_servers = name_servers;
    
    while (iterations < MAX_ITERATIONS) {
        iterations++;
        debugLog("=== Iteration " + std::to_string(iterations) + " ===");
        
        if (current_servers.empty()) {
            debugLog("No more name servers to query");
            return false;
        }
        
        // Ограничение на количество попыток для текущего уровня серверов
        if (server_tries >= MAX_SERVER_TRIES) {
            debugLog("Too many failed attempts for current server level");
            if (!debug_mode_ && iterations == 1) {
                std::cerr << "Error: Unable to reach root DNS servers." << std::endl;
                std::cerr << "This may indicate network restrictions or firewall blocking port 53." << std::endl;
            }
            return false;
        }
        
        // Общее ограничение на количество неудач
        if (total_failures >= MAX_TOTAL_FAILURES) {
            debugLog("Too many total failures, giving up");
            if (!debug_mode_) {
                std::cerr << "Error: Unable to reach DNS servers." << std::endl;
                std::cerr << "Possible reasons:" << std::endl;
                std::cerr << "  - Firewall blocking port 53 (UDP/TCP)" << std::endl;
                std::cerr << "  - Network restrictions preventing direct DNS queries" << std::endl;
                std::cerr << "  - Root DNS servers not accessible from your network" << std::endl;
                std::cerr << "Try running with -d flag for detailed debugging." << std::endl;
            }
            return false;
        }
        
        // Выбираем первый сервер из списка
        std::string server = current_servers[0];
        debugLog("Querying server: " + server);
        
        // Создаем запрос
        uint16_t query_id = static_cast<uint16_t>(time(nullptr) & 0xFFFF);
        std::vector<uint8_t> query = createDNSQuery(query_id, domain, type);
        
        // Пробуем UDP с коротким таймаутом
        std::vector<uint8_t> response;
        bool truncated = false;
        bool success = queryUDP(server, query, response, truncated, query_id);
        
        // Если усечено, переключаемся на TCP
        if (success && truncated) {
            debugLog("UDP response truncated, switching to TCP");
            success = queryTCP(server, query, response, query_id);
        }
        
        if (!success) {
            server_tries++;
            total_failures++;
            debugLog("Query failed, trying next server (attempt " + std::to_string(server_tries) + "/" + std::to_string(MAX_SERVER_TRIES) + ", total failures: " + std::to_string(total_failures) + ")");
            
            // Если серверов больше нет, выходим
            if (current_servers.size() <= 1) {
                debugLog("No more servers to try");
                return false;
            }
            
            current_servers.erase(current_servers.begin());
            continue;
        }
        
        // Сброс счетчиков при успешном ответе
        server_tries = 0;
        total_failures = 0;
        
        // Парсим ответ
        std::vector<DNSResourceRecord> answers;
        std::vector<DNSResourceRecord> authority;
        std::vector<DNSResourceRecord> additional;
        
        if (!parseDNSResponse(response, type, answers, authority, additional)) {
            debugLog("Failed to parse response");
            current_servers.erase(current_servers.begin());
            continue;
        }
        
        // Проверяем ответы
        bool found_final_answer = false;
        std::string cname_target;
        
        for (const auto& rr : answers) {
            if (rr.type == type) {
                if (type == DNSRecordType::A && !rr.ipv4_address.empty()) {
                    results.push_back(rr.ipv4_address);
                    debugLog("Found A record: " + rr.ipv4_address);
                    found_final_answer = true;
                } else if (type == DNSRecordType::AAAA && !rr.ipv6_address.empty()) {
                    results.push_back(rr.ipv6_address);
                    debugLog("Found AAAA record: " + rr.ipv6_address);
                    found_final_answer = true;
                } else if (type == DNSRecordType::NS && !rr.ns_name.empty()) {
                    results.push_back(rr.ns_name);
                    debugLog("Found NS record: " + rr.ns_name);
                    found_final_answer = true;
                }
            } else if (rr.type == DNSRecordType::CNAME && !rr.cname.empty()) {
                cname_target = rr.cname;
                debugLog("Found CNAME record pointing to: " + cname_target);
            }
        }
        
        // Если нашли финальный ответ нужного типа, возвращаем успех
        if (found_final_answer) {
            return true;
        }
        
        // Если нашли CNAME, нужно разрешить его (но с ограничением глубины)
        if (!cname_target.empty() && iterations < MAX_ITERATIONS - 5) {
            debugLog("Following CNAME to: " + cname_target);
            std::vector<std::string> cname_results;
            if (iterativeResolve(cname_target, type, ROOT_SERVERS, cname_results)) {
                results.insert(results.end(), cname_results.begin(), cname_results.end());
                return true;
            }
            debugLog("Failed to resolve CNAME target");
        }
        
        // Ищем NS записи в Authority секции
        std::vector<std::string> next_servers;
        for (const auto& rr : authority) {
            if (rr.type == DNSRecordType::NS && !rr.ns_name.empty()) {
                next_servers.push_back(rr.ns_name);
                debugLog("Found authority NS: " + rr.ns_name);
            }
        }
        
        // Ищем IP адреса для NS серверов в Additional секции
        current_servers.clear();
        for (const auto& rr : additional) {
            if (rr.type == DNSRecordType::A && !rr.ipv4_address.empty()) {
                current_servers.push_back(rr.ipv4_address);
                debugLog("Found NS server IP: " + rr.ipv4_address);
            }
        }
        
        // Если не нашли IP адреса, нужно разрешить имена NS серверов
        if (current_servers.empty() && !next_servers.empty()) {
            debugLog("Need to resolve NS server names");
            // Рекурсивно разрешаем первый NS сервер (с ограничением итераций)
            if (iterations < MAX_ITERATIONS - 3) {
                std::vector<std::string> ns_ips;
                if (iterativeResolve(next_servers[0], DNSRecordType::A, ROOT_SERVERS, ns_ips)) {
                    current_servers = ns_ips;
                    server_tries = 0; // Сброс счетчика при переходе на новый уровень
                } else {
                    debugLog("Failed to resolve NS server name: " + next_servers[0]);
                    // Пробуем следующий NS сервер, если есть
                    if (next_servers.size() > 1 && iterations < MAX_ITERATIONS - 3) {
                        std::vector<std::string> ns_ips2;
                        if (iterativeResolve(next_servers[1], DNSRecordType::A, ROOT_SERVERS, ns_ips2)) {
                            current_servers = ns_ips2;
                            server_tries = 0;
                        } else {
                            debugLog("Failed to resolve second NS server name");
                            return false;
                        }
                    } else {
                        return false;
                    }
                }
            } else {
                debugLog("Too many iterations, cannot resolve NS server names");
                return false;
            }
        }
        
        if (current_servers.empty()) {
            debugLog("No more servers to query");
            return false;
        }
        
        // Сброс счетчика при переходе на новый уровень серверов
        server_tries = 0;
    }
    
    debugLog("Maximum iterations reached");
    return false;
}

bool DNSResolver::resolve(const std::string& domain, DNSRecordType type, std::vector<std::string>& results) {
    results.clear();
    
    if (!isValidDomainName(domain)) {
        std::cerr << "Invalid domain name: " << domain << std::endl;
        return false;
    }
    
    debugLog("Resolving " + domain + " (type: " + recordTypeToString(type) + ")");
    debugLog("Starting from root servers");
    
    return iterativeResolve(domain, type, ROOT_SERVERS, results);
}

void DNSResolver::debugLog(const std::string& message) {
    if (debug_mode_) {
        std::cerr << "[DEBUG] " << message << std::endl;
    }
}

std::string DNSResolver::recordTypeToString(DNSRecordType type) {
    switch (type) {
        case DNSRecordType::A: return "A";
        case DNSRecordType::AAAA: return "AAAA";
        case DNSRecordType::NS: return "NS";
        case DNSRecordType::CNAME: return "CNAME";
        case DNSRecordType::SOA: return "SOA";
        default: return "UNKNOWN";
    }
}

std::string DNSResolver::ipv6ToString(const uint8_t* addr) {
    char ip_str[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, addr, ip_str, INET6_ADDRSTRLEN);
    return std::string(ip_str);
}

// ==================== Main Function ====================

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " [-d] <domain> <type>" << std::endl;
        std::cerr << "  -d       : Enable debug mode" << std::endl;
        std::cerr << "  domain   : Domain name to resolve" << std::endl;
        std::cerr << "  type     : Record type (A, AAAA, NS, etc.)" << std::endl;
        return 1;
    }
    
    bool debug_mode = false;
    std::string domain;
    std::string type_str;
    
    // Парсинг аргументов
    int arg_idx = 1;
    if (argc >= 4 && std::string(argv[1]) == "-d") {
        debug_mode = true;
        arg_idx = 2;
    }
    
    if (arg_idx >= argc) {
        std::cerr << "Error: domain name required" << std::endl;
        return 1;
    }
    domain = argv[arg_idx++];
    
    if (arg_idx >= argc) {
        std::cerr << "Error: record type required" << std::endl;
        return 1;
    }
    type_str = argv[arg_idx];
    
    // Преобразование типа записи
    DNSRecordType type;
    if (type_str == "A") {
        type = DNSRecordType::A;
    } else if (type_str == "AAAA") {
        type = DNSRecordType::AAAA;
    } else if (type_str == "NS") {
        type = DNSRecordType::NS;
    } else if (type_str == "CNAME") {
        type = DNSRecordType::CNAME;
    } else {
        std::cerr << "Unsupported record type: " << type_str << std::endl;
        std::cerr << "Supported types: A, AAAA, NS, CNAME" << std::endl;
        return 1;
    }
    
    // Создание резолвера и разрешение
    DNSResolver resolver(debug_mode);
    std::vector<std::string> results;
    
    if (resolver.resolve(domain, type, results)) {
        if (results.empty()) {
            std::cerr << "No results found" << std::endl;
            return 1;
        }
        
        // Вывод результатов (только в обычном режиме, в debug режиме уже выведено)
        if (!debug_mode) {
            for (const auto& result : results) {
                std::cout << result << std::endl;
            }
        } else {
            std::cout << "Results:" << std::endl;
            for (const auto& result : results) {
                std::cout << "  " << result << std::endl;
            }
        }
        
        return 0;
    } else {
        std::cerr << "Failed to resolve " << domain << std::endl;
        return 1;
    }
}

