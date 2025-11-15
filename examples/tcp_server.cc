#include <chrono>
#include <print>
#include <spw_rmap/internal/tcp_server.hh>
#include <vector>

using namespace std::chrono_literals;

auto main() -> int {
  auto server = spw_rmap::internal::TCPServer("0.0.0.0", "10032");
  {
    auto res = server.accept_once(0ms, 0ms);
    if (!res.has_value()) {
      std::println("Failed to accept a connection. Error: {}",
                   res.error().message());
      return 1;
    }
  }
  std::vector<uint8_t> buffer;
  buffer.resize(1024);

  {
    auto res = server.recvSome(buffer);
    if (!res.has_value()) {
      std::println("Failed to receive data. Error: {}", res.error().message());
      return 1;
    }
  }

  for (const auto& byte : buffer) {
    std::print("{:02x} ", byte);
  }
}
