#include <print>
#include <spw_rmap/internal/tcp_client.hh>
#include <vector>

using namespace std::chrono_literals;

auto main() -> int {
  auto client = spw_rmap::internal::TCPClient("localhost", "10032");
  auto res = client.connect(500ms, 500ms, 500ms);
  if (!res.has_value()) {
    std::println("Failed to connect to the server. Error: {}",
                 res.error().message());
    return 1;
  }
  std::vector<uint8_t> buffer;
  try {
    buffer.resize(1024);
  } catch (const std::bad_alloc& e) {
    std::println("Failed to allocate memory for the buffer: {}", e.what());
    return 1;
  }
  buffer[0] = 0x01;  // Example data to send
  buffer[1] = 0x02;
  buffer[2] = 0x03;
  buffer[3] = 0x04;

  res = client.sendAll(std::span<const uint8_t>(buffer.data(), 4));
  if (!res.has_value()) {
    std::println("Failed to send data. Error: {}", res.error().message());
    return 1;
  }

  for (const auto& byte : buffer) {
    std::print("{:02x} ", byte);
  }
}
