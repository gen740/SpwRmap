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

TEST(PacketParser, ReadReplyPacket) {
  std::vector<uint8_t> replyAddress = {};
  std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
  auto packetBuilder = SpwRmap::ReadReplyPacketBuilder();
  packetBuilder.setConfig({
      .replyAddress = replyAddress,
      .initiatorLogicalAddress = 0x35,
      .status = 0x00,
      .targetLogicalAddress = 0xEF,
      .transactionID = 0x00,
      .data = data,
      .incrementMode = true,
  });
  packetBuilder.build();

  for (const auto& byte : packetBuilder.getPacket()) {
    std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte) << " ";
  }

  try {
    RMAPPacket packet;
    packet.setTransactionID(0x00);
    packet.setReply();
    packet.interpretAsAnRMAPPacket(const_cast<uint8_t*>(packetBuilder.getPacket().data()),
                                   packetBuilder.getPacket().size());

  } catch (CxxUtilities::Exception& e) {
    FAIL() << "Exception thrown during packet parsing: " << e.toString();
  }
}
