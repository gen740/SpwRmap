#include <SpwRmap/internal/TCPClient.hh>
#include <print>
#include <vector>

using namespace std::chrono_literals;

auto main() -> int {
  auto client = SpwRmap::internal::TCPClient("localhost", 10032, 100ms, 100ms);
  std::vector<uint8_t> buffer;
  buffer.resize(1024);

  buffer[0] = 0x01;  // Example data to send
  buffer[1] = 0x02;
  buffer[2] = 0x03;
  buffer[3] = 0x04;

  try {
    client.send_all(std::span<const uint8_t>(buffer.data(), 4));
  } catch (const std::system_error& e) {
    std::println("Error receiving data: {}", e.what());
  }

  for (const auto& byte : buffer) {
    std::print("{:02x} ", byte);
  }
}
