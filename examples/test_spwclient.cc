#include <SpwRmap/SpwRmap.hh>
#include <SpwRmap/testing/SpwServer.hh>
#include <print>

using namespace std::chrono_literals;

auto main() -> int {
  SpwRmap::SpwRmap rmap("localhost", 10032);

  std::println("Connecting to SpaceWire interface at localhost:10032");
  rmap.initialize(1024, 1024);
  std::println("Connected successfully");

  SpwRmap::TargetNodeDynamic target_node{0xFE, {0x00, 0x01, 0x02, 0x03}, {}};

  {
    std::vector<uint8_t> data_to_write(64);
    for (size_t i = 0; i < data_to_write.size(); ++i) {
      data_to_write[i] = static_cast<uint8_t>(i);
    }

    auto res = rmap.write(target_node, 0x00000000, data_to_write);
    if (!res.has_value()) {
      std::println("Write error: {}", res.error().message());
    } else {
      std::println("Write successful");
    }
  }

  {
    std::vector<uint8_t> read_data(64);
    auto res = rmap.read(target_node, 0x00000000, read_data);
    if (!res.has_value()) {
      std::println("Read error: {}", res.error().message());
    } else {
      std::println("Read successful, data:");
      for (const auto &byte : read_data) {
        std::print("{:02X} ", byte);
      }
      std::println();
    }
  }
}
