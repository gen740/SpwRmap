#include <gmock/gmock.h>
#include <gtest/gtest-matchers.h>
#include <gtest/gtest.h>

#include <array>
#include <random>
#include <spw_rmap/packet_builder.hh>
#include <spw_rmap/packet_parser.hh>

#include "spw_rmap/target_node.hh"

auto random_logical_address() {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  return std::uniform_int_distribution<uint8_t>(32, 126)(gen);
}

auto random_bus_address() {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  return std::uniform_int_distribution<uint8_t>(1, 31)(gen);
}

auto random_address() {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  return std::uniform_int_distribution<uint32_t>(0, 0xFFFFFFFF)(gen);
}

auto random_data_length() {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  return std::uniform_int_distribution<uint16_t>(1, 1024)(gen);
}

auto random_byte() {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  return std::uniform_int_distribution<uint8_t>(0, 255)(gen);
}

auto random_bus_length() {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  return std::uniform_int_distribution<size_t>(0, 12)(gen);
}

using ::testing::Eq;
using ::testing::Pointwise;

template <class T>
auto SpanEqual(std::span<const T> a, std::span<const T> b)
    -> ::testing::AssertionResult {
  if (a.size() != b.size()) {
    return ::testing::AssertionFailure()
           << "size mismatch: " << a.size() << " vs " << b.size();
  }
  for (size_t i = 0; i < a.size(); ++i) {
    if (a[i] != b[i]) {
      return ::testing::AssertionFailure()
             << "mismatch at " << i << ": " << +a[i] << " vs " << +b[i];
    }
  }
  return ::testing::AssertionSuccess();
}

TEST(spw_rmap, ReadPacket) {
  using namespace spw_rmap;

  for (int i = 0; i < 1000; ++i) {
    std::vector<uint8_t> target_address;
    std::vector<uint8_t> reply_address;

    for (size_t i = 0; i < random_bus_length(); ++i) {
      target_address.push_back(random_bus_address());
      reply_address.push_back(random_bus_address());
    }

    TargetNodeDynamic node(random_logical_address(), std::move(target_address),
                           std::move(reply_address));

    auto b = ReadPacketBuilder();
    auto c = ReadPacketConfig{
        .targetSpaceWireAddress = node.getTargetSpaceWireAddress(),
        .replyAddress = node.getReplyAddress(),
        .targetLogicalAddress = node.getTargetLogicalAddress(),
        .initiatorLogicalAddress = random_logical_address(),
        .transactionID =
            static_cast<uint16_t>(random_byte() << 8 | random_byte()),
        .extendedAddress = random_byte(),
        .address = random_address(),
        .dataLength = random_data_length(),
        .key = random_byte(),
        .incrementMode = random_byte() % 2 == 0,
    };

    std::vector<uint8_t> packet;
    packet.resize(b.getTotalSize(c));

    auto res = b.build(c, packet);
    ASSERT_TRUE(res.has_value());
    auto parser = PacketParser();
    auto parsed = parser.parse(packet);
    ASSERT_TRUE(parsed == PacketParser::Status::Success);

    auto d = parser.getPacket();
    EXPECT_TRUE(SpanEqual(d.targetSpaceWireAddress, c.targetSpaceWireAddress));
    EXPECT_TRUE(SpanEqual(d.replyAddress, c.replyAddress));
    EXPECT_EQ(d.targetLogicalAddress, c.targetLogicalAddress);
    EXPECT_EQ(d.initiatorLogicalAddress, c.initiatorLogicalAddress);
    EXPECT_EQ(d.transactionID, c.transactionID);
    EXPECT_EQ(d.extendedAddress, c.extendedAddress);
    EXPECT_EQ(d.address, c.address);
    EXPECT_EQ(d.dataLength, c.dataLength);
    EXPECT_EQ(d.key, c.key);
    EXPECT_EQ(d.type, PacketType::Read);
    EXPECT_EQ(d.instruction & 0b00000100, c.incrementMode ? 0b00000100 : 0);
  }
}

TEST(spw_rmap, ReadReplyPacket) {
  using namespace spw_rmap;

  for (int i = 0; i < 1000; ++i) {
    std::vector<uint8_t> target_address;
    std::vector<uint8_t> reply_address;

    for (size_t i = 0; i < random_bus_length(); ++i) {
      target_address.push_back(random_bus_address());
      reply_address.push_back(random_bus_address());
    }

    TargetNodeDynamic node(random_logical_address(), std::move(target_address),
                           std::move(reply_address));

    std::vector<uint8_t> data;

    for (size_t i = 0; i < random_data_length(); ++i) {
      data.push_back(random_byte());
    }

    auto b = ReadReplyPacketBuilder();
    auto c = ReadReplyPacketConfig{
        .replyAddress = node.getReplyAddress(),
        .status = random_byte(),
        .targetLogicalAddress = node.getTargetLogicalAddress(),
        .transactionID =
            static_cast<uint16_t>(random_byte() << 8 | random_byte()),
        .data = data,
        .incrementMode = random_byte() % 2 == 0,
    };

    std::vector<uint8_t> packet;
    packet.resize(b.getTotalSize(c));

    auto res = b.build(c, packet);
    ASSERT_TRUE(res.has_value());
    auto parser = PacketParser();
    auto parsed = parser.parse(packet);
    ASSERT_TRUE(parsed == PacketParser::Status::Success);

    auto d = parser.getPacket();
    EXPECT_TRUE(SpanEqual(d.replyAddress, c.replyAddress));
    EXPECT_EQ(d.status, c.status);
    EXPECT_EQ(d.targetLogicalAddress, c.targetLogicalAddress);
    EXPECT_EQ(d.transactionID, c.transactionID);
    EXPECT_TRUE(SpanEqual(d.data, c.data));
    EXPECT_EQ(d.type, PacketType::ReadReply);
    EXPECT_EQ(d.instruction & 0b00000100, c.incrementMode ? 0b00000100 : 0);
  }
}

TEST(spw_rmap, WritePacket) {
  using namespace spw_rmap;

  for (int i = 0; i < 1000; ++i) {
    std::vector<uint8_t> target_address;
    std::vector<uint8_t> reply_address;

    for (size_t i = 0; i < random_bus_length(); ++i) {
      target_address.push_back(random_bus_address());
      reply_address.push_back(random_bus_address());
    }

    TargetNodeDynamic node(random_logical_address(), std::move(target_address),
                           std::move(reply_address));

    std::vector<uint8_t> data;

    for (size_t i = 0; i < random_data_length(); ++i) {
      data.push_back(random_byte());
    }

    auto b = WritePacketBuilder();
    auto c = WritePacketConfig{
        .targetSpaceWireAddress = node.getTargetSpaceWireAddress(),
        .replyAddress = node.getReplyAddress(),
        .targetLogicalAddress = node.getTargetLogicalAddress(),
        .initiatorLogicalAddress = random_logical_address(),
        .transactionID =
            static_cast<uint16_t>(random_byte() << 8 | random_byte()),
        .key = random_byte(),
        .extendedAddress = random_byte(),
        .address = random_address(),
        .incrementMode = random_byte() % 2 == 0,
        .verifyMode = random_byte() % 2 == 0,
        .data = data,
    };

    std::vector<uint8_t> packet;
    packet.resize(b.getTotalSize(c));

    auto res = b.build(c, packet);
    ASSERT_TRUE(res.has_value());
    auto parser = PacketParser();
    auto parsed = parser.parse(packet);
    ASSERT_TRUE(parsed == PacketParser::Status::Success);

    auto d = parser.getPacket();
    EXPECT_TRUE(SpanEqual(d.targetSpaceWireAddress, c.targetSpaceWireAddress));
    EXPECT_TRUE(SpanEqual(d.replyAddress, c.replyAddress));
    EXPECT_EQ(d.targetLogicalAddress, c.targetLogicalAddress);
    EXPECT_EQ(d.initiatorLogicalAddress, c.initiatorLogicalAddress);
    EXPECT_EQ(d.transactionID, c.transactionID);
    EXPECT_EQ(d.key, c.key);
    EXPECT_EQ(d.extendedAddress, c.extendedAddress);
    EXPECT_EQ(d.address, c.address);
    EXPECT_EQ(d.instruction & 0b00000100, c.incrementMode ? 0b00000100 : 0);
    EXPECT_TRUE(SpanEqual(d.data, c.data));
  }
}

TEST(spw_rmap, WriteReplyPacket) {
  using namespace spw_rmap;

  for (int i = 0; i < 1000; ++i) {
    std::vector<uint8_t> target_address;
    std::vector<uint8_t> reply_address;

    for (size_t i = 0; i < random_bus_length(); ++i) {
      target_address.push_back(random_bus_address());
      reply_address.push_back(random_bus_address());
    }

    TargetNodeDynamic node(random_logical_address(), std::move(target_address),
                           std::move(reply_address));

    auto b = WriteReplyPacketBuilder();
    auto c = WriteReplyPacketConfig{
        .replyAddress = node.getReplyAddress(),
        .initiatorLogicalAddress = random_logical_address(),
        .status = random_byte(),
        .targetLogicalAddress = node.getTargetLogicalAddress(),
        .transactionID =
            static_cast<uint16_t>(random_byte() << 8 | random_byte()),
        .incrementMode = random_byte() % 2 == 0,
        .verifyMode = random_byte() % 2 == 0,
    };

    std::vector<uint8_t> packet;
    packet.resize(b.getTotalSize(c));

    auto res = b.build(c, packet);
    ASSERT_TRUE(res.has_value());
    auto parser = PacketParser();
    auto parsed = parser.parse(packet);
    ASSERT_TRUE(parsed == PacketParser::Status::Success);

    auto d = parser.getPacket();
    EXPECT_TRUE(SpanEqual(d.replyAddress, c.replyAddress));
    EXPECT_EQ(d.initiatorLogicalAddress, c.initiatorLogicalAddress);
    EXPECT_EQ(d.status, c.status);
    EXPECT_EQ(d.targetLogicalAddress, c.targetLogicalAddress);
    EXPECT_EQ(d.transactionID, c.transactionID);
    EXPECT_EQ(d.type, PacketType::WriteReply);
    EXPECT_EQ(d.instruction & 0b00000100, c.incrementMode ? 0b00000100 : 0);
  }
}

TEST(spw_rmap, ParseReadPacketDirectly) {
  using namespace spw_rmap;

  std::vector<uint8_t> target_address{3, 5, 7};
  std::vector<uint8_t> reply_address{9, 11, 13};

  TargetNodeDynamic node(0x34, std::move(target_address),
                         std::move(reply_address));

  auto builder = ReadPacketBuilder();
  auto config = ReadPacketConfig{
      .targetSpaceWireAddress = node.getTargetSpaceWireAddress(),
      .replyAddress = node.getReplyAddress(),
      .targetLogicalAddress = node.getTargetLogicalAddress(),
      .initiatorLogicalAddress = 0xA1,
      .transactionID = 0x1234,
      .extendedAddress = 0x02,
      .address = 0x01020304,
      .dataLength = 32,
      .key = 0x55,
      .incrementMode = true,
  };

  std::vector<uint8_t> packet(builder.getTotalSize(config));
  ASSERT_TRUE(builder.build(config, packet).has_value());

  PacketParser parser;
  const auto status = parser.parseReadPacket(
      std::span(packet).subspan(node.getTargetSpaceWireAddress().size()));
  ASSERT_EQ(status, PacketParser::Status::Success);

  const auto& parsed = parser.getPacket();
  EXPECT_TRUE(SpanEqual(parsed.replyAddress, config.replyAddress));
  EXPECT_EQ(parsed.targetLogicalAddress, config.targetLogicalAddress);
  EXPECT_EQ(parsed.initiatorLogicalAddress, config.initiatorLogicalAddress);
  EXPECT_EQ(parsed.transactionID, config.transactionID);
  EXPECT_EQ(parsed.extendedAddress, config.extendedAddress);
  EXPECT_EQ(parsed.address, config.address);
  EXPECT_EQ(parsed.dataLength, config.dataLength);
  EXPECT_EQ(parsed.key, config.key);
}

TEST(spw_rmap, ParseWritePacketDirectly) {
  using namespace spw_rmap;

  std::vector<uint8_t> target_address{1, 2};
  std::vector<uint8_t> reply_address{3, 4, 5, 6};

  TargetNodeDynamic node(0x56, std::move(target_address),
                         std::move(reply_address));

  std::array<uint8_t, 8> payload{0, 1, 2, 3, 4, 5, 6, 7};

  auto builder = WritePacketBuilder();
  auto config = WritePacketConfig{
      .targetSpaceWireAddress = node.getTargetSpaceWireAddress(),
      .replyAddress = node.getReplyAddress(),
      .targetLogicalAddress = node.getTargetLogicalAddress(),
      .initiatorLogicalAddress = 0xB2,
      .transactionID = 0x4321,
      .key = 0xAA,
      .extendedAddress = 0x01,
      .address = 0x0A0B0C0D,
      .incrementMode = false,
      .reply = true,
      .verifyMode = true,
      .data = payload,
  };

  std::vector<uint8_t> packet(builder.getTotalSize(config));
  ASSERT_TRUE(builder.build(config, packet).has_value());

  PacketParser parser;
  const auto status = parser.parseWritePacket(
      std::span(packet).subspan(node.getTargetSpaceWireAddress().size()));
  ASSERT_EQ(status, PacketParser::Status::Success);

  const auto& parsed = parser.getPacket();
  EXPECT_TRUE(SpanEqual(parsed.replyAddress, config.replyAddress));
  EXPECT_EQ(parsed.targetLogicalAddress, config.targetLogicalAddress);
  EXPECT_EQ(parsed.initiatorLogicalAddress, config.initiatorLogicalAddress);
  EXPECT_EQ(parsed.transactionID, config.transactionID);
  EXPECT_EQ(parsed.key, config.key);
  EXPECT_EQ(parsed.extendedAddress, config.extendedAddress);
  EXPECT_EQ(parsed.address, config.address);
  EXPECT_TRUE(SpanEqual(parsed.data, config.data));
}
