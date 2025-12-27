#pragma once

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace rdt {

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

inline std::string errno_message(std::string_view prefix) {
  return std::string(prefix) + ": " + std::strerror(errno);
}

// ---- CRC32 (IEEE 802.3, polynomial 0xEDB88320) ------------------------------
inline uint32_t crc32(const uint8_t* data, size_t len) {
  static uint32_t table[256];
  static bool inited = false;
  if (!inited) {
    for (uint32_t i = 0; i < 256; ++i) {
      uint32_t c = i;
      for (int k = 0; k < 8; ++k) {
        c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
      }
      table[i] = c;
    }
    inited = true;
  }

  uint32_t c = 0xFFFFFFFFu;
  for (size_t i = 0; i < len; ++i) {
    c = table[(c ^ data[i]) & 0xFFu] ^ (c >> 8);
  }
  return c ^ 0xFFFFFFFFu;
}

// ---- Packet format -----------------------------------------------------------
// Wire header is fixed-size and in network byte order.
//
// [magic:4]["RDTP"] [version:1] [type:1] [reserved:2]
// [seq:4] [len:2] [hdrLen:2] [crc32:4]  => total 20 bytes
//
// crc32 is calculated over (header with crc32 field set to 0) + payload bytes.
constexpr uint32_t kMagic = 0x52445450u;  // 'RDTP'
constexpr uint8_t kVersion = 1;
constexpr size_t kHeaderSize = 20;

enum class PacketType : uint8_t { Data = 1, Ack = 2, Fin = 3 };

struct ParsedPacket {
  PacketType type{};
  uint32_t seq = 0;         // For ACK: ack number (cumulative); for DATA/FIN: seq number.
  std::vector<uint8_t> payload;
};

inline std::vector<uint8_t> build_packet(PacketType type, uint32_t seq,
                                         const uint8_t* payload, uint16_t len) {
  std::vector<uint8_t> buf(kHeaderSize + len);

  auto put_u32 = [&](size_t off, uint32_t v) {
    uint32_t nv = htonl(v);
    std::memcpy(buf.data() + off, &nv, sizeof(nv));
  };
  auto put_u16 = [&](size_t off, uint16_t v) {
    uint16_t nv = htons(v);
    std::memcpy(buf.data() + off, &nv, sizeof(nv));
  };

  put_u32(0, kMagic);
  buf[4] = kVersion;
  buf[5] = static_cast<uint8_t>(type);
  put_u16(6, 0);  // reserved
  put_u32(8, seq);
  put_u16(12, len);
  put_u16(14, static_cast<uint16_t>(kHeaderSize));
  put_u32(16, 0);  // crc placeholder

  if (len > 0 && payload != nullptr) {
    std::memcpy(buf.data() + kHeaderSize, payload, len);
  }

  uint32_t c = crc32(buf.data(), buf.size());
  put_u32(16, c);
  return buf;
}

inline std::optional<ParsedPacket> parse_packet(const uint8_t* buf, size_t n) {
  if (n < kHeaderSize) return std::nullopt;

  auto get_u32 = [&](size_t off) -> uint32_t {
    uint32_t v;
    std::memcpy(&v, buf + off, sizeof(v));
    return ntohl(v);
  };
  auto get_u16 = [&](size_t off) -> uint16_t {
    uint16_t v;
    std::memcpy(&v, buf + off, sizeof(v));
    return ntohs(v);
  };

  const uint32_t magic = get_u32(0);
  if (magic != kMagic) return std::nullopt;
  if (buf[4] != kVersion) return std::nullopt;

  const uint8_t type_u8 = buf[5];
  if (type_u8 != static_cast<uint8_t>(PacketType::Data) &&
      type_u8 != static_cast<uint8_t>(PacketType::Ack) &&
      type_u8 != static_cast<uint8_t>(PacketType::Fin)) {
    return std::nullopt;
  }

  const uint32_t seq = get_u32(8);
  const uint16_t len = get_u16(12);
  const uint16_t hdr_len = get_u16(14);
  if (hdr_len != kHeaderSize) return std::nullopt;
  if (static_cast<size_t>(kHeaderSize) + len != n) return std::nullopt;

  // Verify CRC32
  uint32_t got_crc;
  std::memcpy(&got_crc, buf + 16, sizeof(got_crc));
  got_crc = ntohl(got_crc);

  std::vector<uint8_t> tmp(buf, buf + n);
  uint32_t zero = 0;
  zero = htonl(zero);
  std::memcpy(tmp.data() + 16, &zero, sizeof(zero));
  uint32_t calc = crc32(tmp.data(), tmp.size());
  if (calc != got_crc) return std::nullopt;

  ParsedPacket out;
  out.type = static_cast<PacketType>(type_u8);
  out.seq = seq;
  out.payload.assign(buf + kHeaderSize, buf + kHeaderSize + len);
  return out;
}

// ---- Networking helpers ------------------------------------------------------
inline UniqueFd udp_socket() {
  // "Best effort" dual-stack socket (platform dependent). Good default for listeners.
  int fd = ::socket(AF_INET6, SOCK_DGRAM, 0);
  if (fd >= 0) {
    int off = 0;
    (void)::setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
    return UniqueFd(fd);
  }
  fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) throw std::runtime_error(errno_message("socket"));
  return UniqueFd(fd);
}

inline UniqueFd udp_socket_for_family(int family) {
  int fd = ::socket(family, SOCK_DGRAM, 0);
  if (fd < 0) throw std::runtime_error(errno_message("socket"));
  if (family == AF_INET6) {
    int off = 0;
    (void)::setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
  }
  return UniqueFd(fd);
}

inline sockaddr_storage resolve_remote(const std::string& host, const std::string& port,
                                       socklen_t* out_len) {
  addrinfo hints{};
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_family = AF_UNSPEC;

  addrinfo* res = nullptr;
  int rc = ::getaddrinfo(host.c_str(), port.c_str(), &hints, &res);
  if (rc != 0) throw std::runtime_error(std::string("getaddrinfo: ") + gai_strerror(rc));

  sockaddr_storage ss{};
  std::memcpy(&ss, res->ai_addr, res->ai_addrlen);
  *out_len = static_cast<socklen_t>(res->ai_addrlen);
  ::freeaddrinfo(res);
  return ss;
}

inline void bind_local(int fd, const std::string& port) {
  addrinfo hints{};
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_family = AF_UNSPEC;
  hints.ai_flags = AI_PASSIVE;

  addrinfo* res = nullptr;
  int rc = ::getaddrinfo(nullptr, port.c_str(), &hints, &res);
  if (rc != 0) throw std::runtime_error(std::string("getaddrinfo: ") + gai_strerror(rc));

  bool ok = false;
  for (addrinfo* p = res; p != nullptr; p = p->ai_next) {
    int on = 1;
    (void)::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if (::bind(fd, p->ai_addr, p->ai_addrlen) == 0) {
      ok = true;
      break;
    }
  }
  ::freeaddrinfo(res);
  if (!ok) throw std::runtime_error(errno_message("bind"));
}

inline uint64_t now_ms() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

inline timeval ms_to_timeval(uint64_t ms) {
  timeval tv{};
  tv.tv_sec = static_cast<time_t>(ms / 1000);
  tv.tv_usec = static_cast<suseconds_t>((ms % 1000) * 1000);
  return tv;
}

inline void print_rate(uint64_t bytes, uint64_t elapsed_ms) {
  if (elapsed_ms == 0) elapsed_ms = 1;
  double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
  double s = static_cast<double>(elapsed_ms) / 1000.0;
  double mbps = (mb * 8.0) / s;
  std::cerr << std::fixed << std::setprecision(2) << mb << " MiB in " << s << " s ("
            << mbps << " Mib/s)\n";
}

}  // namespace rdt


