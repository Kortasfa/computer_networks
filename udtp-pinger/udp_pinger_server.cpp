#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>

namespace {

class UniqueFd {
 public:
  UniqueFd() = default;
  explicit UniqueFd(int fd) : fd_(fd) {}
  UniqueFd(const UniqueFd&) = delete;
  UniqueFd& operator=(const UniqueFd&) = delete;
  UniqueFd(UniqueFd&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }
  UniqueFd& operator=(UniqueFd&& other) noexcept {
    if (this != &other) {
      reset();
      fd_ = other.fd_;
      other.fd_ = -1;
    }
    return *this;
  }
  ~UniqueFd() { reset(); }

  int get() const { return fd_; }
  explicit operator bool() const { return fd_ >= 0; }

  void reset(int new_fd = -1) {
    if (fd_ >= 0) ::close(fd_);
    fd_ = new_fd;
  }

 private:
  int fd_ = -1;
};

std::string addr_to_string(const sockaddr* sa, socklen_t salen) {
  char host[NI_MAXHOST];
  char serv[NI_MAXSERV];
  int rc = ::getnameinfo(sa, salen, host, sizeof(host), serv, sizeof(serv),
                         NI_NUMERICHOST | NI_NUMERICSERV);
  if (rc != 0) return "<unknown>";
  std::string out(host);
  out += ":";
  out += serv;
  return out;
}

void print_usage(const char* argv0) {
  std::cerr
      << "Usage:\n"
      << "  " << argv0
      << " <port> [--loss-percent P] [--delay-ms MS] "
         "[--heartbeat-timeout-sec N]\n"
      << "\n"
      << "Options:\n"
      << "  --loss-percent P            Drop ~P% of incoming packets (0..100)\n"
      << "  --delay-ms MS               Artificial delay before reply\n"
      << "  --heartbeat-timeout-sec N   If no packets from a client for N seconds, "
         "print \"client died\" message\n";
}

bool is_flag(const std::string& s, const char* flag) { return s == flag; }

struct ClientInfo {
  std::chrono::steady_clock::time_point last_seen;
  bool dead_reported = false;
};

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    print_usage(argv[0]);
    return 1;
  }

  int port = 0;
  try {
    port = std::stoi(argv[1]);
  } catch (...) {
    std::cerr << "Invalid port: " << argv[1] << "\n";
    return 2;
  }
  if (port <= 0 || port > 65535) {
    std::cerr << "Invalid port: " << port << "\n";
    return 2;
  }

  double loss_percent = 0.0;
  int delay_ms = 0;
  int heartbeat_timeout_sec = 0;

  for (int i = 2; i < argc; i++) {
    std::string a = argv[i];
    auto require_value = [&](const char* flag) -> std::string {
      if (i + 1 >= argc) {
        std::cerr << "Missing value for " << flag << "\n";
        std::exit(2);
      }
      return argv[++i];
    };

    if (is_flag(a, "--loss-percent")) {
      loss_percent = std::stod(require_value("--loss-percent"));
    } else if (is_flag(a, "--delay-ms")) {
      delay_ms = std::stoi(require_value("--delay-ms"));
    } else if (is_flag(a, "--heartbeat-timeout-sec")) {
      heartbeat_timeout_sec = std::stoi(require_value("--heartbeat-timeout-sec"));
    } else if (is_flag(a, "--help") || is_flag(a, "-h")) {
      print_usage(argv[0]);
      return 0;
    } else {
      std::cerr << "Unknown argument: " << a << "\n";
      print_usage(argv[0]);
      return 2;
    }
  }

  if (loss_percent < 0.0 || loss_percent > 100.0) {
    std::cerr << "Invalid --loss-percent (must be 0..100)\n";
    return 2;
  }
  if (delay_ms < 0) {
    std::cerr << "Invalid --delay-ms (must be >= 0)\n";
    return 2;
  }
  if (heartbeat_timeout_sec < 0) {
    std::cerr << "Invalid --heartbeat-timeout-sec (must be >= 0)\n";
    return 2;
  }

  UniqueFd fd(::socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP));
  if (!fd) {
    perror("socket");
    return 1;
  }

  // Allow IPv4-mapped IPv6 addresses, so one socket works for both.
  int v6only = 0;
  ::setsockopt(fd.get(), IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only));

  int reuse = 1;
  ::setsockopt(fd.get(), SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  sockaddr_in6 addr{};
  addr.sin6_family = AF_INET6;
  addr.sin6_addr = in6addr_any;
  addr.sin6_port = htons(static_cast<uint16_t>(port));

  if (::bind(fd.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    perror("bind");
    return 1;
  }

  // 1s timeout so we can periodically check heartbeat liveness.
  timeval tv{};
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  ::setsockopt(fd.get(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  std::mt19937 rng(std::random_device{}());
  std::uniform_real_distribution<double> dist01(0.0, 100.0);

  std::unordered_map<std::string, ClientInfo> clients;

  std::cout << "[udp_pinger_server] Listening on UDP port " << port << "\n";
  if (loss_percent > 0.0) {
    std::cout << "[udp_pinger_server] loss-percent=" << loss_percent << "\n";
  }
  if (delay_ms > 0) {
    std::cout << "[udp_pinger_server] delay-ms=" << delay_ms << "\n";
  }
  if (heartbeat_timeout_sec > 0) {
    std::cout << "[udp_pinger_server] heartbeat-timeout-sec="
              << heartbeat_timeout_sec << "\n";
  }
  std::cout.flush();

  while (true) {
    char buf[1024];
    sockaddr_storage from{};
    socklen_t from_len = sizeof(from);
    ssize_t n = ::recvfrom(fd.get(), buf, sizeof(buf) - 1, 0,
                           reinterpret_cast<sockaddr*>(&from), &from_len);
    if (n < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
        perror("recvfrom");
        return 1;
      }
      // timeout or interrupted: fallthrough to heartbeat checks
    } else {
      buf[n] = '\0';
      std::string payload(buf);
      std::string key = addr_to_string(reinterpret_cast<sockaddr*>(&from), from_len);

      auto now = std::chrono::steady_clock::now();
      ClientInfo& ci = clients[key];
      ci.last_seen = now;
      ci.dead_reported = false;

      bool drop = dist01(rng) < loss_percent;
      if (!drop) {
        if (delay_ms > 0) {
          std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        }
        ssize_t sent = ::sendto(fd.get(), payload.data(), payload.size(), 0,
                                reinterpret_cast<sockaddr*>(&from), from_len);
        if (sent < 0) {
          perror("sendto");
        }
      }
    }

    if (heartbeat_timeout_sec > 0) {
      auto now = std::chrono::steady_clock::now();
      for (auto& [key, ci] : clients) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - ci.last_seen);
        if (!ci.dead_reported && elapsed.count() > heartbeat_timeout_sec) {
          std::cout << "[heartbeat] Client " << key << " died (no packets for "
                    << elapsed.count() << "s)\n";
          std::cout.flush();
          ci.dead_reported = true;
        }
      }
    }
  }
}


