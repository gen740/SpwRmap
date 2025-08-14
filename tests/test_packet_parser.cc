#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "RMAPPacket.hh"
#include "SpwRmap/PacketBuilder.hh"

TEST(PacketParser, ReadReplyPacket) {
  std::vector<uint8_t> replyAddress = {};
  std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
  auto packetBuilder = SpwRmap::ReadReplyPacketBuilder();
  {
    auto res = packetBuilder.build({
        .replyAddress = replyAddress,
        .initiatorLogicalAddress = 0x35,
        .status = 0x00,
        .targetLogicalAddress = 0xEF,
        .transactionID = 0x00,
        .data = data,
        .incrementMode = true,
    });
    if (!res.has_value()) {
      FAIL() << "Failed to build read reply packet: " << res.error().message();
    }
  }
  try {
    RMAPPacket packet;
    packet.setTransactionID(0x00);
    packet.setReply();
    packet.interpretAsAnRMAPPacket(
        const_cast<uint8_t*>(packetBuilder.getPacket()->data()),  // NOLINT
        packetBuilder.getPacket()->size());

  } catch (CxxUtilities::Exception& e) {
    FAIL() << "Exception thrown during packet parsing: " << e.toString();
  }
}
