#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

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
    if (fd_ >= 0) {
      ::close(fd_);
    }
    fd_ = new_fd;
  }

 private:
  int fd_ = -1;
};

long long now_ms_since_epoch() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

std::string addr_to_string(const sockaddr* sa, socklen_t salen) {
  char host[NI_MAXHOST];
  char serv[NI_MAXSERV];
  int rc = ::getnameinfo(sa, salen, host, sizeof(host), serv, sizeof(serv),
                         NI_NUMERICHOST | NI_NUMERICSERV);
  if (rc != 0) {
    return "<unknown>";
  }
  std::string out(host);
  out += ":";
  out += serv;
  return out;
}

bool resolve_udp_endpoint(const std::string& host,
                          const std::string& port,
                          sockaddr_storage& out_addr,
                          socklen_t& out_len,
                          int& out_family) {
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = IPPROTO_UDP;

  addrinfo* res = nullptr;
  int rc = ::getaddrinfo(host.c_str(), port.c_str(), &hints, &res);
  if (rc != 0) {
    std::cerr << "getaddrinfo: " << ::gai_strerror(rc) << "\n";
    return false;
  }

  bool ok = false;
  for (addrinfo* p = res; p != nullptr; p = p->ai_next) {
    if (p->ai_addr == nullptr) continue;
    if (p->ai_addrlen > sizeof(sockaddr_storage)) continue;
    if (p->ai_family != AF_INET && p->ai_family != AF_INET6) continue;
    std::memset(&out_addr, 0, sizeof(out_addr));
    std::memcpy(&out_addr, p->ai_addr, p->ai_addrlen);
    out_len = static_cast<socklen_t>(p->ai_addrlen);
    out_family = p->ai_family;
    ok = true;
    break;
  }
  ::freeaddrinfo(res);
  if (!ok) {
    std::cerr << "Failed to resolve UDP endpoint for " << host << ":" << port
              << "\n";
  }
  return ok;
}

bool parse_ping_payload(const std::string& payload,
                        int& seq_out,
                        long long& ts_ms_out) {
  std::istringstream iss(payload);
  std::string kind;
  if (!(iss >> kind >> seq_out >> ts_ms_out)) return false;
  if (kind != "Ping" && kind != "Heartbeat") return false;
  return true;
}

void print_usage(const char* argv0) {
  std::cerr
      << "Usage:\n"
      << "  " << argv0
      << " <host> <port> [--count N] [--timeout-ms MS] [--interval-ms MS]\n"
      << "  " << argv0
      << " --heartbeat <host> <port> [--count N] [--timeout-ms MS] "
         "[--interval-ms MS]\n"
      << "\n"
      << "Defaults: count=10, timeout-ms=1000, interval-ms=1000\n";
}

bool is_flag(const std::string& s, const char* flag) { return s == flag; }

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    print_usage(argv[0]);
    return 1;
  }

  bool heartbeat_mode = false;
  std::string host;
  std::string port;
  int count = 10;
  int timeout_ms = 1000;
  int interval_ms = 1000;

  int i = 1;
  if (argc >= 2 && std::string(argv[1]) == "--heartbeat") {
    heartbeat_mode = true;
    i++;
  }
  if (i + 1 >= argc) {
    print_usage(argv[0]);
    return 1;
  }
  host = argv[i++];
  port = argv[i++];

  while (i < argc) {
    std::string a = argv[i++];
    auto require_value = [&](const char* flag) -> std::string {
      if (i >= argc) {
        std::cerr << "Missing value for " << flag << "\n";
        std::exit(2);
      }
      return argv[i++];
    };

    if (is_flag(a, "--count")) {
      count = std::stoi(require_value("--count"));
    } else if (is_flag(a, "--timeout-ms")) {
      timeout_ms = std::stoi(require_value("--timeout-ms"));
    } else if (is_flag(a, "--interval-ms")) {
      interval_ms = std::stoi(require_value("--interval-ms"));
    } else if (is_flag(a, "--help") || is_flag(a, "-h")) {
      print_usage(argv[0]);
      return 0;
    } else {
      std::cerr << "Unknown argument: " << a << "\n";
      print_usage(argv[0]);
      return 2;
    }
  }

  if (count <= 0) {
    std::cerr << "Invalid --count (must be > 0)\n";
    return 2;
  }
  if (timeout_ms <= 0) {
    std::cerr << "Invalid --timeout-ms (must be > 0)\n";
    return 2;
  }
  if (interval_ms < 0) {
    std::cerr << "Invalid --interval-ms (must be >= 0)\n";
    return 2;
  }

  sockaddr_storage server_addr{};
  socklen_t server_len = 0;
  int family = AF_UNSPEC;
  if (!resolve_udp_endpoint(host, port, server_addr, server_len, family)) {
    return 1;
  }

  UniqueFd fd(::socket(family, SOCK_DGRAM, IPPROTO_UDP));
  if (!fd) {
    perror("socket");
    return 1;
  }

  timeval tv{};
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;
  if (::setsockopt(fd.get(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
    perror("setsockopt(SO_RCVTIMEO)");
    return 1;
  }

  std::string server_str =
      addr_to_string(reinterpret_cast<sockaddr*>(&server_addr), server_len);
  std::cout << "[udp_pinger] Target " << server_str << "\n";

  int received = 0;
  int lost = 0;
  std::vector<double> rtts_ms;
  rtts_ms.reserve(static_cast<size_t>(count));

  for (int seq = 1; seq <= count; seq++) {
    long long ts_ms = now_ms_since_epoch();
    std::ostringstream oss;
    oss << (heartbeat_mode ? "Heartbeat" : "Ping") << " " << seq << " " << ts_ms;
    std::string msg = oss.str();

    ssize_t sent = ::sendto(fd.get(), msg.data(), msg.size(), 0,
                            reinterpret_cast<const sockaddr*>(&server_addr),
                            server_len);
    if (sent < 0) {
      perror("sendto");
      return 1;
    }

    char buf[1024];
    sockaddr_storage from{};
    socklen_t from_len = sizeof(from);
    ssize_t n = ::recvfrom(fd.get(), buf, sizeof(buf) - 1, 0,
                           reinterpret_cast<sockaddr*>(&from), &from_len);
    if (n < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        std::cout << "Request timed out (seq=" << seq << ")\n";
        lost++;
      } else if (errno == EINTR) {
        std::cout << "Interrupted (seq=" << seq << ")\n";
        lost++;
      } else {
        perror("recvfrom");
        return 1;
      }
    } else {
      buf[n] = '\0';
      std::string payload(buf);

      int resp_seq = 0;
      long long resp_ts_ms = 0;
      if (!parse_ping_payload(payload, resp_seq, resp_ts_ms)) {
        std::cout << "Received malformed response (seq=" << seq
                  << "): \"" << payload << "\"\n";
        lost++;
      } else {
        long long now_ms = now_ms_since_epoch();
        long long rtt_ll = now_ms - resp_ts_ms;
        double rtt = static_cast<double>(rtt_ll);
        rtts_ms.push_back(rtt);
        received++;
        std::string from_str =
            addr_to_string(reinterpret_cast<sockaddr*>(&from), from_len);
        std::cout << "Reply from " << from_str << ": seq=" << resp_seq
                  << " time=" << resp_ts_ms << " rtt=" << rtt << " ms\n";
      }
    }

    if (seq != count && interval_ms > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }
  }

  std::cout << "\n--- UDP Pinger statistics ---\n";
  std::cout << count << " packets transmitted, " << received << " received, "
            << lost << " lost ("
            << (count > 0 ? (100.0 * lost / count) : 0.0) << "% loss)\n";

  if (!rtts_ms.empty()) {
    double min_rtt = std::numeric_limits<double>::infinity();
    double max_rtt = 0.0;
    double sum = 0.0;
    for (double v : rtts_ms) {
      if (v < min_rtt) min_rtt = v;
      if (v > max_rtt) max_rtt = v;
      sum += v;
    }
    double avg = sum / rtts_ms.size();
    std::cout << "rtt min/avg/max = " << min_rtt << "/" << avg << "/" << max_rtt
              << " ms\n";
  } else {
    std::cout << "No RTT samples (all packets lost).\n";
  }

  return 0;
}


