#include <gtest/gtest.h>

#include <spw_rmap/packet_builder.hh>
#include <spw_rmap/packet_parser.hh>

#include "spw_rmap/target_node.hh"

TEST(spw_rmap, Packet) {
  using namespace spw_rmap;

  TargetNodeFixed<4, 4> node(0x43, {0x01, 0x02, 0x03, 0x04},
                             {0x05, 0x06, 0x07, 0x08});

  auto builder = ReadPacketBuilder();
  auto config = ReadPacketConfig{
      .targetSpaceWireAddress = node.getTargetSpaceWireAddress(),
      .replyAddress = node.getReplyAddress(),
      .targetLogicalAddress = node.getTargetLogicalAddress(),
      .initiatorLogicalAddress = 0xFE,
      .transactionID = 0x1234,
      .extendedAddress = 0x00,
      .address = 0x00000000,
      .dataLength = 16,
      .key = 0x00,
      .incrementMode = true,
  };

  std::vector<uint8_t> packet;
  packet.resize(builder.getTotalSize(config));

  auto res = builder.build(config, packet);
  ASSERT_TRUE(res.has_value());
  auto parser = PacketParser();
  auto parsed = parser.parse(packet);
  ASSERT_TRUE(parsed == PacketParser::Status::Success);

  auto d = parser.getPacket();
  EXPECT_EQ(d.targetLogicalAddress, config.targetLogicalAddress);
  EXPECT_EQ(d.initiatorLogicalAddress, config.initiatorLogicalAddress);
  EXPECT_EQ(d.transactionID, config.transactionID);
  EXPECT_EQ(d.extendedAddress, config.extendedAddress);
  EXPECT_EQ(d.address, config.address);
  EXPECT_EQ(d.dataLength, config.dataLength);
  EXPECT_EQ(d.key, config.key);
}
