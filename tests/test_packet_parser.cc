#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <random>
#include <thread>

#include "RMAPEngine.hh"
#include "RMAPInitiator.hh"
#include "SpaceWireIF.hh"
// #include "Packe
#include "SpwRmap/PacketBuilder.hh"

struct writeReplyPacketConfig {
  uint8_t initiatorLogicalAddress;
  uint8_t status;
  uint8_t targetLogicalAddress;
  uint16_t transactionID;
};

auto makeWriteReplyPacket(writeReplyPacketConfig config) -> std::vector<uint8_t> {
  std::vector<uint8_t> packet;
  packet.push_back(config.initiatorLogicalAddress);
  packet.push_back(0x01);        // Protocol Identifier
  packet.push_back(0b00001110);  // RMAP packet type (Write Reply)
  packet.push_back(config.status);
  packet.push_back(config.targetLogicalAddress);
  packet.push_back(config.transactionID >> 8);    // Transaction ID (high byte)
  packet.push_back(config.transactionID & 0xFF);  // Transaction ID (low byte)
  packet.push_back(0x00);                         // Reserved byte
  auto crc = SpwRmap::calcCRC(std::span(packet));
  packet.push_back(crc);
  return packet;
}

// TEST(PacketParser, WriteReplyPacket) {
//   auto config = writeReplyPacketConfig{
//       .initiatorLogicalAddress = 0x35,
//       .status = 0x00,
//       .targetLogicalAddress = 0xEF,
//       .transactionID = 0x0000,
//   };
//   auto packet = makeWriteReplyPacket(config);
//   RMAPPacket legacy_packet;
//   legacy_packet.interpretAsAnRMAPPacket(packet);
// }
