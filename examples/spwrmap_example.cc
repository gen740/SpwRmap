#include <chrono>
#include <iostream>
#include <spw_rmap/spw_rmap_tcp_node.hh>

#include "spw_rmap/packet_parser.hh"
#include "spw_rmap/target_node.hh"

using namespace std::chrono_literals;

auto main() -> int {
  auto spw = spw_rmap::SpwRmapTCPClient(
      {.ip_address = "192.168.1.100", .port = "10030"});
  spw.setInitiatorLogicalAddress(0xFE);

  auto res_con = spw.connect(1s);

  if (res_con.has_value()) {
    std::cout << "Connected to SpaceWire RMAP TCP Node." << std::endl;
  } else {
    std::cerr << "Connection error: " << res_con.error().message() << std::endl;
    return 1;
  }

  auto target = std::make_shared<spw_rmap::TargetNodeDynamic>(
      0x32, std::vector<uint8_t>{2}, std::vector<uint8_t>{3});

  std::thread main_thread([&spw]() -> void { std::ignore = spw.runLoop(); });
  std::array<uint8_t, 4> data_to_write = {0x01, 0x02, 0x03, 0x04};
  std::future<std::expected<std::monostate, std::error_code>> future;

  future = spw.writeAsync(target, 0x00000000, data_to_write,
                          [](spw_rmap::Packet) -> void {
                            std::cout << "Write callback called." << std::endl;
                            return;
                          });
  future.wait();
  if (future.get().has_value()) {
    std::cout << "Write successful." << std::endl;
  } else {
    std::cerr << "Write error: " << future.get().error().message() << std::endl;
  }

  std::vector<uint8_t> buf = {};
  future = spw.readAsync(
      target, 0x00000000, 4, [&buf](spw_rmap::Packet packet) -> void {
        std::cout << "Read callback called." << std::endl;
        std::ranges::copy(packet.data, std::back_inserter(buf));
        return;
      });
  future.wait();

  if (future.get().has_value()) {
    std::cout << "Read successful. Data: ";
    for (const auto& byte : buf) {
      std::cout << std::hex << static_cast<int>(byte) << " ";
    }
    std::cout << std::dec << std::endl;
  } else {
    std::cerr << "Read error: " << future.get().error().message() << std::endl;
  }

  auto res = spw.shutdown();
  if (res.has_value()) {
    main_thread.join();
    std::cout << "Shutdown successfully." << std::endl;
  } else {
    std::cerr << "Shutdown error: " << res.error().message() << std::endl;
  }
}
