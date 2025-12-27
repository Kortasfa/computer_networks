#include "rdt_common.hpp"

#include <fstream>
#include <deque>

namespace {

struct Options {
  std::string host;
  std::string port;
  std::string file;
  uint16_t mss = 1000;
  uint32_t window = 64;
  uint64_t timeout_ms = 200;
};

[[noreturn]] void usage(const char* argv0) {
  std::cerr
      << "Usage:\n"
      << "  " << argv0 << " <host> <port> <file_to_send>\n"
      << "Optional:\n"
      << "  -w <window_packets>   (default 64)\n"
      << "  -t <timeout_ms>       (default 200)\n"
      << "  -m <mss_bytes>         (default 1000)\n";
  std::exit(2);
}

Options parse_args(int argc, char** argv) {
  Options opt;
  int i = 1;
  while (i < argc && std::string_view(argv[i]).rfind("-", 0) == 0) {
    std::string_view a(argv[i]);
    if (a == "-w" && i + 1 < argc) {
      opt.window = static_cast<uint32_t>(std::stoul(argv[++i]));
    } else if (a == "-t" && i + 1 < argc) {
      opt.timeout_ms = static_cast<uint64_t>(std::stoull(argv[++i]));
    } else if (a == "-m" && i + 1 < argc) {
      opt.mss = static_cast<uint16_t>(std::stoul(argv[++i]));
    } else {
      usage(argv[0]);
    }
    ++i;
  }

  if (argc - i != 3) usage(argv[0]);
  opt.host = argv[i];
  opt.port = argv[i + 1];
  opt.file = argv[i + 2];
  if (opt.mss == 0 || opt.mss > 1400) {
    throw std::runtime_error("Bad MSS: choose 1..1400 (to fit UDP MTU safely)");
  }
  if (opt.window == 0) throw std::runtime_error("Window must be > 0");
  if (opt.timeout_ms < 10) throw std::runtime_error("Timeout too small");
  return opt;
}

struct InFlight {
  uint32_t seq = 0;
  std::vector<uint8_t> bytes;
  uint64_t last_send_ms = 0;
  uint16_t payload_len = 0;
};

}  // namespace

int main(int argc, char** argv) {
  try {
    Options opt = parse_args(argc, argv);

    std::ifstream in(opt.file, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open input file: " + opt.file);

    socklen_t peer_len = 0;
    sockaddr_storage peer = rdt::resolve_remote(opt.host, opt.port, &peer_len);
    rdt::UniqueFd sock = rdt::udp_socket_for_family(peer.ss_family);

    std::deque<InFlight> window;
    uint32_t base = 0;
    uint32_t next_seq = 0;
    bool eof = false;

    uint64_t total_sent_payload = 0;
    uint64_t retransmits = 0;
    uint64_t start_ms = rdt::now_ms();

    auto send_raw = [&](const std::vector<uint8_t>& bytes) {
      ssize_t rc = ::sendto(sock.get(), bytes.data(), bytes.size(), 0,
                            reinterpret_cast<const sockaddr*>(&peer), peer_len);
      if (rc < 0) throw std::runtime_error(rdt::errno_message("sendto"));
    };

    auto send_data_packet = [&](InFlight& p) {
      send_raw(p.bytes);
      p.last_send_ms = rdt::now_ms();
    };

    auto fill_window = [&]() {
      while (!eof && window.size() < opt.window) {
        std::vector<uint8_t> payload(opt.mss);
        in.read(reinterpret_cast<char*>(payload.data()), payload.size());
        std::streamsize got = in.gcount();
        if (got <= 0) {
          eof = true;
          break;
        }
        payload.resize(static_cast<size_t>(got));

        InFlight p;
        p.seq = next_seq++;
        p.payload_len = static_cast<uint16_t>(payload.size());
        p.bytes = rdt::build_packet(rdt::PacketType::Data, p.seq, payload.data(),
                                    static_cast<uint16_t>(payload.size()));

        send_data_packet(p);
        total_sent_payload += payload.size();
        window.push_back(std::move(p));
      }
    };

    auto handle_ack = [&](uint32_t ackno) {
      while (!window.empty() && window.front().seq <= ackno) {
        window.pop_front();
        base = ackno + 1;
      }
    };

    fill_window();

    std::vector<uint8_t> rxbuf(2048);
    while (!eof || !window.empty()) {
      uint64_t timeout = opt.timeout_ms;
      if (!window.empty()) {
        uint64_t elapsed = rdt::now_ms() - window.front().last_send_ms;
        timeout = (elapsed >= opt.timeout_ms) ? 0 : (opt.timeout_ms - elapsed);
      }

      fd_set rfds;
      FD_ZERO(&rfds);
      FD_SET(sock.get(), &rfds);
      timeval tv = rdt::ms_to_timeval(timeout);

      int rc = ::select(sock.get() + 1, &rfds, nullptr, nullptr, &tv);
      if (rc < 0) {
        if (errno == EINTR) continue;
        throw std::runtime_error(rdt::errno_message("select"));
      }

      if (rc == 0) {
        for (auto& p : window) {
          send_data_packet(p);
          ++retransmits;
        }
        continue;
      }

      if (FD_ISSET(sock.get(), &rfds)) {
        sockaddr_storage from{};
        socklen_t from_len = sizeof(from);
        ssize_t n = ::recvfrom(sock.get(), rxbuf.data(), rxbuf.size(), 0,
                               reinterpret_cast<sockaddr*>(&from), &from_len);
        if (n < 0) {
          if (errno == EINTR) continue;
          throw std::runtime_error(rdt::errno_message("recvfrom"));
        }

        auto pkt = rdt::parse_packet(rxbuf.data(), static_cast<size_t>(n));
        if (!pkt) continue;
        if (pkt->type != rdt::PacketType::Ack) continue;

        uint32_t ackno = pkt->seq;
        if (!window.empty() && ackno >= window.front().seq) {
          handle_ack(ackno);
          fill_window();
        }
      }
    }

    const uint32_t fin_seq = next_seq;
    auto fin = rdt::build_packet(rdt::PacketType::Fin, fin_seq, nullptr, 0);

    bool fin_acked = false;
    uint64_t last_fin_send = 0;
    while (!fin_acked) {
      uint64_t now = rdt::now_ms();
      if (now - last_fin_send >= opt.timeout_ms) {
        send_raw(fin);
        last_fin_send = now;
      }

      fd_set rfds;
      FD_ZERO(&rfds);
      FD_SET(sock.get(), &rfds);
      timeval tv = rdt::ms_to_timeval(opt.timeout_ms);

      int rc = ::select(sock.get() + 1, &rfds, nullptr, nullptr, &tv);
      if (rc < 0) {
        if (errno == EINTR) continue;
        throw std::runtime_error(rdt::errno_message("select"));
      }
      if (rc == 0) continue;

      ssize_t n = ::recvfrom(sock.get(), rxbuf.data(), rxbuf.size(), 0, nullptr, nullptr);
      if (n < 0) {
        if (errno == EINTR) continue;
        throw std::runtime_error(rdt::errno_message("recvfrom"));
      }
      auto pkt = rdt::parse_packet(rxbuf.data(), static_cast<size_t>(n));
      if (!pkt) continue;
      if (pkt->type == rdt::PacketType::Ack && pkt->seq == fin_seq) {
        fin_acked = true;
      }
    }

    uint64_t elapsed = rdt::now_ms() - start_ms;
    std::cerr << "RDTP sender finished.\n";
    std::cerr << "  payload bytes read: " << total_sent_payload << "\n";
    std::cerr << "  retransmits: " << retransmits << "\n";
    std::cerr << "  rate: ";
    rdt::print_rate(total_sent_payload, elapsed);
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}


