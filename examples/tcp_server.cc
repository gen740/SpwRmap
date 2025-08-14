#include <SpwRmap/internal/TCPServer.hh>
#include <chrono>
#include <print>
#include <vector>

using namespace std::chrono_literals;

auto main() -> int {
  auto server = SpwRmap::internal::TCPServer("0.0.0.0", "10032");
  auto res = server.accept_once(500ms, 500ms);
  if (!res.has_value()) {
    std::println("Failed to accept a connection. Error: {}",
                 res.error().message());
    return 1;
  }
  std::vector<uint8_t> buffer;
  buffer.resize(1024);

  try {
    server.recv_some(buffer);
  } catch (const std::system_error& e) {
    std::println("Error receiving data: {}", e.what());
  }

  for (const auto& byte : buffer) {
    std::print("{:02x} ", byte);
  }
}
