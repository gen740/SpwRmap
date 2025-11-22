#include <array>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "spw_rmap/spw_rmap_tcp_node.hh"
#include "spw_rmap/target_node.hh"

using namespace std::chrono_literals;

auto main() -> int try {
  // Connect to the SpaceWire/RMAP bridge.
  spw_rmap::SpwRmapTCPClient client(
      {.ip_address = "192.168.1.100", .port = "10030"});
  client.setInitiatorLogicalAddress(0xFE);
  if (auto res = client.connect(1s); !res.has_value()) {
    std::cerr << "Connect failed: " << res.error().message() << '\n';
    return 1;
  }
  std::cout << "Connected to RMAP bridge\n";

  // Create a target node describing the remote logical/SpaceWire addresses.
  auto target = std::make_shared<spw_rmap::TargetNodeDynamic>(
      0x32, std::vector<uint8_t>{0x06, 0x02}, std::vector<uint8_t>{0x01, 0x03});

  // Run the receive loop on a background thread so replies are processed.
  std::thread loop([&client]() {
    auto res = client.runLoop();
    if (!res.has_value()) {
      std::cerr << "runLoop error: " << res.error().message() << '\n';
    }
  });

  const uint32_t demo_address = 0x44A20000;
  const std::array<uint8_t, 4> payload{0x01, 0x02, 0x03, 0x04};

  // Blocking write then read back into a local buffer.
  bool keep_running = true;
  if (auto res = client.write(target, demo_address, payload);
      !res.has_value()) {
    std::cerr << "Sync write failed: " << res.error().message() << '\n';
    keep_running = false;
  } else {
    std::array<uint8_t, 4> buffer{};
    if (auto res = client.read(target, demo_address, std::span(buffer));
        res.has_value()) {
      std::cout << "Sync read: ";
      for (auto byte : buffer) {
        std::cout << "0x" << std::hex << +byte << ' ';
      }
      std::cout << std::dec << '\n';
    } else {
      std::cerr << "Sync read failed: " << res.error().message() << '\n';
      keep_running = false;
    }
  }

  if (keep_running) {
    auto write_future = client.writeAsync(
        target, demo_address, payload, [](const spw_rmap::Packet&) {
          std::cout << "Async write completed\n";
        });
    if (!write_future.get().has_value()) {
      std::cerr << "Async write failed\n";
      keep_running = false;
    }
  }

  if (keep_running) {
    auto read_future = client.readAsync(
        target, demo_address, payload.size(), [](spw_rmap::Packet packet) {
          std::cout << "Async read returned " << packet.data.size()
                    << " bytes\n";
        });
    if (!read_future.get().has_value()) {
      std::cerr << "Async read failed\n";
    }
  }

  if (auto res = client.shutdown(); !res.has_value()) {
    std::cerr << "Shutdown error: " << res.error().message() << '\n';
  }
  if (loop.joinable()) {
    loop.join();
  }
  return 0;
} catch (const std::exception& e) {
  std::cerr << "Exception: " << e.what() << '\n';
  return 1;
}
