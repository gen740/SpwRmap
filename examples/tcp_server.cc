#include <SpwRmap/internal/TCPServer.hh>
#include <chrono>
#include <print>
#include <vector>

using namespace std::chrono_literals;

int main() {
  auto server = SpwRmap::internal::TCPServer("0.0.0.0", 10032, 100ms, 100ms);
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
