#ifndef DNS_RESOLVER_HPP
#define DNS_RESOLVER_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// DNS типы записей
enum class DNSRecordType : uint16_t {
    A = 1,
    NS = 2,
    CNAME = 5,
    SOA = 6,
    AAAA = 28
};

// DNS классы
enum class DNSClass : uint16_t {
    IN = 1
};

// DNS структуры согласно RFC 1035
#pragma pack(push, 1)
struct DNSHeader {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
};

struct DNSQuestion {
    // QNAME - переменной длины, обрабатывается отдельно
    uint16_t qtype;
    uint16_t qclass;
};
#pragma pack(pop)

// DNS Resource Record
struct DNSResourceRecord {
    std::string name;
    DNSRecordType type;
    DNSClass class_;
    uint32_t ttl;
    uint16_t data_length;
    std::vector<uint8_t> data;
    
    // Распарсенные данные в зависимости от типа
    std::string ipv4_address;  // для A записи
    std::string ipv6_address;  // для AAAA записи
    std::string ns_name;       // для NS записи
    std::string cname;         // для CNAME записи
};

// RAII обертка для UDP сокета
class UDPSocket {
public:
    UDPSocket();
    ~UDPSocket();
    
    // Запрещаем копирование
    UDPSocket(const UDPSocket&) = delete;
    UDPSocket& operator=(const UDPSocket&) = delete;
    
    // Разрешаем перемещение
    UDPSocket(UDPSocket&& other) noexcept;
    UDPSocket& operator=(UDPSocket&& other) noexcept;
    
    bool isValid() const { return sockfd_ >= 0; }
    int getFd() const { return sockfd_; }
    
    bool sendTo(const std::string& server_ip, uint16_t port, const void* data, size_t len);
    ssize_t recvFrom(void* buffer, size_t len, std::string& from_ip, uint16_t& from_port, int timeout_sec = 5);
    
private:
    int sockfd_;
};

// RAII обертка для TCP сокета
class TCPSocket {
public:
    TCPSocket();
    ~TCPSocket();
    
    // Запрещаем копирование
    TCPSocket(const TCPSocket&) = delete;
    TCPSocket& operator=(const TCPSocket&) = delete;
    
    // Разрешаем перемещение
    TCPSocket(TCPSocket&& other) noexcept;
    TCPSocket& operator=(TCPSocket&& other) noexcept;
    
    bool isValid() const { return sockfd_ >= 0; }
    int getFd() const { return sockfd_; }
    
    bool connectTo(const std::string& server_ip, uint16_t port);
    ssize_t send(const void* data, size_t len);
    ssize_t recv(void* buffer, size_t len, int timeout_sec = 5);
    
private:
    int sockfd_;
};

// DNS резолвер
class DNSResolver {
public:
    DNSResolver(bool debug_mode = false);
    
    // Основной метод разрешения
    bool resolve(const std::string& domain, DNSRecordType type, std::vector<std::string>& results);
    
private:
    bool debug_mode_;
    
    // Корневые DNS серверы
    static const std::vector<std::string> ROOT_SERVERS;
    
    // Создание DNS запроса
    std::vector<uint8_t> createDNSQuery(uint16_t id, const std::string& domain, DNSRecordType type);
    
    // Парсинг DNS ответа
    bool parseDNSResponse(const std::vector<uint8_t>& response, 
                         DNSRecordType query_type,
                         std::vector<DNSResourceRecord>& answers,
                         std::vector<DNSResourceRecord>& authority,
                         std::vector<DNSResourceRecord>& additional);
    
    // Парсинг имени из DNS пакета (с поддержкой компрессии)
    std::string parseDNSName(const std::vector<uint8_t>& packet, size_t& offset);
    
    // Парсинг одной Resource Record
    bool parseResourceRecord(const std::vector<uint8_t>& packet, size_t& offset, DNSResourceRecord& rr);
    
    // Отправка запроса через UDP
    bool queryUDP(const std::string& server_ip, const std::vector<uint8_t>& query, 
                  std::vector<uint8_t>& response, bool& truncated, uint16_t expected_id);
    
    // Отправка запроса через TCP
    bool queryTCP(const std::string& server_ip, const std::vector<uint8_t>& query, 
                  std::vector<uint8_t>& response, uint16_t expected_id);
    
    // Итеративное разрешение
    bool iterativeResolve(const std::string& domain, DNSRecordType type, 
                        const std::vector<std::string>& name_servers,
                        std::vector<std::string>& results);
    
    // Вспомогательные функции
    void debugLog(const std::string& message);
    std::string recordTypeToString(DNSRecordType type);
    std::string ipv6ToString(const uint8_t* addr);
    
    // Валидация доменного имени
    bool isValidDomainName(const std::string& domain);
};

#endif // DNS_RESOLVER_HPP

