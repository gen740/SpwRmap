#include <RMAPInitiator.hh>
#include <RMAPPacket.hh>
#include <SpaceWire.hh>
#include <print>
#include <thread>
//
// #include "SpwRmap/CRC.hh"
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
  packet.push_back(0b00101110);  // RMAP packet type (Write Reply)
  packet.push_back(config.status);
  packet.push_back(config.targetLogicalAddress);
  packet.push_back((config.transactionID >> 8) & 0xFF);  // Transaction ID (high byte)
  packet.push_back((config.transactionID >> 0) & 0xFF);  // Transaction ID (low byte)
  auto crc = SpwRmap::calcCRC(packet);
  packet.push_back(crc);
  return packet;
}

int main() {
  auto config = writeReplyPacketConfig{
      .initiatorLogicalAddress = 0x35,
      .status = 0x00,
      .targetLogicalAddress = 0xEF,
      .transactionID = 0x0000,
  };
  auto packet = makeWriteReplyPacket(config);
  RMAPPacket legacy_packet;
  try {
    legacy_packet.interpretAsAnRMAPPacket(packet);
    legacy_packet.isReply();
  } catch (RMAPPacketException& e) {
    std::cerr << "Failed to interpret packet: " << e.toString() << std::endl;
    return 1;
  }
}
