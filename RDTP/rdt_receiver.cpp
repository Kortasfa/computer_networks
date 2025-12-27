#include "rdt_common.hpp"

#include <fstream>

namespace {

struct Options {
  std::string port;
  std::string out_file;
};

[[noreturn]] void usage(const char* argv0) {
  std::cerr << "Usage:\n"
            << "  " << argv0 << " <port> <output_file>\n";
  std::exit(2);
}

Options parse_args(int argc, char** argv) {
  if (argc != 3) usage(argv[0]);
  Options o;
  o.port = argv[1];
  o.out_file = argv[2];
  return o;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    Options opt = parse_args(argc, argv);

    std::ofstream out(opt.out_file, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Cannot open output file: " + opt.out_file);

    rdt::UniqueFd sock;
    try {
      sock = rdt::udp_socket_for_family(AF_INET6);
      rdt::bind_local(sock.get(), opt.port);
    } catch (...) {
      sock = rdt::udp_socket_for_family(AF_INET);
      rdt::bind_local(sock.get(), opt.port);
    }

    std::cerr << "RDTP receiver listening on port " << opt.port << "\n";

    std::vector<uint8_t> rxbuf(64 * 1024);
    uint32_t expected = 0;
    uint32_t last_in_order_ack = static_cast<uint32_t>(-1);  // means "no data yet"

    sockaddr_storage peer{};
    socklen_t peer_len = 0;
    bool have_peer = false;

    auto send_ack = [&](uint32_t ackno) {
      if (!have_peer) return;
      auto ack = rdt::build_packet(rdt::PacketType::Ack, ackno, nullptr, 0);
      ssize_t rc = ::sendto(sock.get(), ack.data(), ack.size(), 0,
                            reinterpret_cast<const sockaddr*>(&peer), peer_len);
      if (rc < 0) throw std::runtime_error(rdt::errno_message("sendto(ack)"));
    };

    uint64_t bytes_written = 0;
    uint64_t start_ms = rdt::now_ms();

    while (true) {
      sockaddr_storage from{};
      socklen_t from_len = sizeof(from);
      ssize_t n = ::recvfrom(sock.get(), rxbuf.data(), rxbuf.size(), 0,
                             reinterpret_cast<sockaddr*>(&from), &from_len);
      if (n < 0) {
        if (errno == EINTR) continue;
        throw std::runtime_error(rdt::errno_message("recvfrom"));
      }

      auto pkt = rdt::parse_packet(rxbuf.data(), static_cast<size_t>(n));
      if (!pkt) {
        continue;
      }

      if (!have_peer) {
        peer = from;
        peer_len = from_len;
        have_peer = true;
      }

      if (pkt->type == rdt::PacketType::Data) {
        if (pkt->seq == expected) {
          if (!pkt->payload.empty()) {
            out.write(reinterpret_cast<const char*>(pkt->payload.data()),
                      static_cast<std::streamsize>(pkt->payload.size()));
            if (!out) throw std::runtime_error("Write failed");
            bytes_written += pkt->payload.size();
          }
          last_in_order_ack = expected;
          ++expected;
          send_ack(last_in_order_ack);
        } else {
          if (last_in_order_ack != static_cast<uint32_t>(-1)) {
            send_ack(last_in_order_ack);
          }
        }
        continue;
      }

      if (pkt->type == rdt::PacketType::Fin) {
        if (pkt->seq == expected) {
          send_ack(pkt->seq);
          break;
        }
        if (last_in_order_ack != static_cast<uint32_t>(-1)) {
          send_ack(last_in_order_ack);
        }
        continue;
      }

    }

    out.flush();
    if (!out) throw std::runtime_error("Flush failed");

    uint64_t elapsed = rdt::now_ms() - start_ms;
    std::cerr << "RDTP receiver finished.\n";
    std::cerr << "  bytes written: " << bytes_written << "\n";
    std::cerr << "  rate: ";
    rdt::print_rate(bytes_written, elapsed);
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}


