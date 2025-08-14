
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "SpwRmap/PacketBuilder.hh"

int main() {
  std::array<uint8_t, 4> targetSpaceWireAddress = {0x01, 0x02, 0x03, 0x04};
  std::array<uint8_t, 4> replyAddress = {0x05, 0x06, 0x07, 0x08};
  SpwRmap::ReadPacketConfig config = {
      .targetSpaceWireAddress = targetSpaceWireAddress,
      .replyAddress = replyAddress,
      .targetLogicalAddress = 0xF2,
      .initiatorLogicalAddress = 0x35,
      .transactionID = 0x1234,
      .extendedAddress = 0x00,
      .address = 0x44A40000,
      .dataLength = 0x00000004,
      .key = 0xAB,
      .incrementMode = true};
  SpwRmap::ReadPacketBuilder read_packet_builder(config);
  read_packet_builder.build();

}
