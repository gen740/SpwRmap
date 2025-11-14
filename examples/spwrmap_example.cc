#include <chrono>
#include <iostream>
#include <spw_rmap/spw_rmap_tcp_node.hh>

#include "spw_rmap/target_node.hh"

auto main() -> int {
  auto spw = spw_rmap::SpwRmapTCPNode(
      {.ip_address = "192.168.1.100", .port = "10030"});
  spw.setInitiatorLogicalAddress(0xFE);
  auto res_con = spw.connect(std::chrono::microseconds(1000000),
                             std::chrono::microseconds(1000000),
                             std::chrono::microseconds(1000000));

  if (res_con.has_value()) {
    std::cout << "Connected to SpaceWire RMAP TCP Node." << std::endl;
  } else {
    std::cerr << "Connection error: " << res_con.error().message() << std::endl;
    return 1;
  }
  auto target = spw_rmap::TargetNodeDynamic(0x32, std::vector<uint8_t>{2},
                                            std::vector<uint8_t>{3});

  std::vector<uint8_t> buf = {};
  buf.resize(4);
  auto res = spw.read(target, 0x44a2006C, buf);

  if (res.has_value()) {
    for (const auto& byte : buf) {
      std::cout << std::hex << static_cast<int>(byte) << " ";
    }
    std::cout << std::dec << std::endl;
  } else {
    std::cerr << "Read error: " << res.error().message() << std::endl;
  }
}
