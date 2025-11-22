#include <array>
#include <iomanip>
#include <iostream>
#include <vector>

#include "spw_rmap/packet_builder.hh"

namespace {

void printPacket(const std::vector<uint8_t>& packet) {
  std::cout << "Packet bytes:";
  for (auto byte : packet) {
    std::cout << " 0x" << std::hex << std::uppercase << static_cast<int>(byte);
  }
  std::cout << std::dec << '\n';
}

}  // namespace

auto main() -> int {
  std::array<uint8_t, 4> target_address{0x01, 0x02, 0x03, 0x04};
  std::array<uint8_t, 4> reply_address{0x05, 0x06, 0x07, 0x08};

  spw_rmap::ReadPacketConfig config{
      .targetSpaceWireAddress = target_address,
      .replyAddress = reply_address,
      .targetLogicalAddress = 0xF2,
      .initiatorLogicalAddress = 0x35,
      .transactionID = 0x1234,
      .extendedAddress = 0x00,
      .address = 0x44A40000,
      .dataLength = 0x00000004,
      .key = 0xAB,
      .incrementMode = true};

  spw_rmap::ReadPacketBuilder builder;
  std::vector<uint8_t> buffer(builder.getTotalSize(config));
  auto res = builder.build(config, buffer);
  if (!res.has_value()) {
    std::cerr << "Failed to build packet: " << res.error().message() << '\n';
    return 1;
  }

  printPacket(buffer);
  return 0;
}
