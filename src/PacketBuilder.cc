#include "SpwRmap/PacketBuilder.hh"

#include <cassert>

#include "SpwRmap/CRC.hh"
#include "SpwRmap/RMAPPacketType.hh"

namespace SpwRmap {

auto ReadPacketBuilder::build() -> void {
  getMutablePacket_().clear();
  for (const auto& byte : getConfig_().targetSpaceWireAddress) {
    getMutablePacket_().push_back(byte);
  }
  getMutablePacket_().push_back(getConfig_().targetLogicalAddress);
  getMutablePacket_().push_back(RMAPProtocolIdentifier);  // Protocol Identifier
  auto replyAddressSize = getConfig_().replyAddress.size();
  {  // Instruction field
    uint8_t instruction = 0;
    instruction |= std::to_underlying(RMAPPacketType::Command);
    instruction |= std::to_underlying(RMAPCommandCode::Reply);
    if (getConfig_().incrementMode) {
      instruction |= std::to_underlying(RMAPCommandCode::IncrementAddress);
    }
    if (replyAddressSize != 0) {
      assert(replyAddressSize <= 12);
      replyAddressSize = ((replyAddressSize - 1) & 0x0C) + 0x04;  // Convert to 4-byte words
      instruction |= (replyAddressSize >> 2);
    }
    getMutablePacket_().push_back(instruction);
  }
  getMutablePacket_().push_back(getConfig_().key);
  if (replyAddressSize != 0) {
    for (size_t i = 0; i < replyAddressSize - getConfig_().replyAddress.size(); ++i) {
      getMutablePacket_().push_back(0x00);
    }
  }
  for (const auto& byte : getConfig_().replyAddress) {
    getMutablePacket_().push_back(byte);
  }
  getMutablePacket_().push_back(getConfig_().initiatorLogicalAddress);
  getMutablePacket_().push_back(static_cast<uint8_t>(getConfig_().transactionID >> 8));
  getMutablePacket_().push_back(static_cast<uint8_t>(getConfig_().transactionID & 0xFF));
  getMutablePacket_().push_back(getConfig_().extendedAddress);
  getMutablePacket_().push_back(static_cast<uint8_t>((getConfig_().address >> 24) & 0xFF));
  getMutablePacket_().push_back(static_cast<uint8_t>((getConfig_().address >> 16) & 0xFF));
  getMutablePacket_().push_back(static_cast<uint8_t>((getConfig_().address >> 8) & 0xFF));
  getMutablePacket_().push_back(static_cast<uint8_t>((getConfig_().address >> 0) & 0xFF));
  getMutablePacket_().push_back(static_cast<uint8_t>((getConfig_().dataLength >> 16) & 0xFF));
  getMutablePacket_().push_back(static_cast<uint8_t>((getConfig_().dataLength >> 8) & 0xFF));
  getMutablePacket_().push_back(static_cast<uint8_t>((getConfig_().dataLength >> 0) & 0xFF));
  auto crc = CRC::calcCRC(
      std::span(getMutablePacket_()).subspan(getConfig_().targetSpaceWireAddress.size()));
  getMutablePacket_().push_back(crc);
};

auto WritePacketBuilder::build() -> void {
  getMutablePacket_().clear();
  for (const auto& byte : getConfig_().targetSpaceWireAddress) {
    getMutablePacket_().push_back(byte);
  }
  getMutablePacket_().push_back(getConfig_().targetLogicalAddress);
  getMutablePacket_().push_back(RMAPProtocolIdentifier);
  auto replyAddressSize = getConfig_().replyAddress.size();
  {  // Instruction field
    uint8_t instruction = 0;
    instruction |= std::to_underlying(RMAPPacketType::Command);
    instruction |= (std::to_underlying(RMAPCommandCode::Write));
    if (getConfig_().reply) {
      instruction |= std::to_underlying(RMAPCommandCode::Reply);
    }
    if (getConfig_().verifyMode) {
      instruction |= std::to_underlying(RMAPCommandCode::VerifyDataBeforeWrite);
    }
    if (getConfig_().incrementMode) {
      instruction |= std::to_underlying(RMAPCommandCode::IncrementAddress);
    }
    if (replyAddressSize != 0) {
      assert(replyAddressSize <= 12);
      replyAddressSize = ((replyAddressSize - 1) & 0x0C) + 0x04;  // Convert to 4-byte words
      instruction |= (replyAddressSize >> 2);
    }
    getMutablePacket_().push_back(instruction);
  }
  getMutablePacket_().push_back(getConfig_().key);
  if (replyAddressSize != 0) {
    for (size_t i = 0; i < replyAddressSize - getConfig_().replyAddress.size(); ++i) {
      getMutablePacket_().push_back(0x00);
    }
  }
  for (const auto& byte : getConfig_().replyAddress) {
    getMutablePacket_().push_back(byte);
  }
  getMutablePacket_().push_back(getConfig_().initiatorLogicalAddress);
  getMutablePacket_().push_back(static_cast<uint8_t>(getConfig_().transactionID >> 8));
  getMutablePacket_().push_back(static_cast<uint8_t>(getConfig_().transactionID & 0xFF));
  getMutablePacket_().push_back(getConfig_().extendedAddress);
  getMutablePacket_().push_back(static_cast<uint8_t>((getConfig_().address >> 24) & 0xFF));
  getMutablePacket_().push_back(static_cast<uint8_t>((getConfig_().address >> 16) & 0xFF));
  getMutablePacket_().push_back(static_cast<uint8_t>((getConfig_().address >> 8) & 0xFF));
  getMutablePacket_().push_back(static_cast<uint8_t>((getConfig_().address >> 0) & 0xFF));

  auto dataLength = getConfig_().data.size();
  getMutablePacket_().push_back(static_cast<uint8_t>((dataLength >> 16) & 0xFF));
  getMutablePacket_().push_back(static_cast<uint8_t>((dataLength >> 8) & 0xFF));
  getMutablePacket_().push_back(static_cast<uint8_t>((dataLength >> 0) & 0xFF));

  auto crc = CRC::calcCRC(
      std::span(getMutablePacket_()).subspan(getConfig_().targetSpaceWireAddress.size()));
  getMutablePacket_().push_back(crc);

  // Append data
  for (const auto& byte : getConfig_().data) {
    getMutablePacket_().push_back(byte);
  }
  auto data_crc = CRC::calcCRC(std::span(getConfig_().data));
  getMutablePacket_().push_back(data_crc);
};

auto WriteReplyPacketBuilder::build() -> void {
  getMutablePacket_().clear();
  for (const auto& byte : getConfig_().replyAddress) {
    getMutablePacket_().push_back(byte);
  }
  getMutablePacket_().push_back(getConfig_().initiatorLogicalAddress);
  getMutablePacket_().push_back(0x01);  // Protocol Identifier
  {                                     // Instruction field
    uint8_t instruction = 0;
    instruction |= (std::to_underlying(RMAPPacketType::Reply));
    instruction |= (std::to_underlying(RMAPCommandCode::Write));
    instruction |= std::to_underlying(RMAPCommandCode::Reply);
    if (getConfig_().verifyMode) {
      instruction |= std::to_underlying(RMAPCommandCode::VerifyDataBeforeWrite);
    }
    if (getConfig_().incrementMode) {
      instruction |= std::to_underlying(RMAPCommandCode::IncrementAddress);
    }
    getMutablePacket_().push_back(instruction);
  }
  getMutablePacket_().push_back(getConfig_().status);
  getMutablePacket_().push_back(getConfig_().targetLogicalAddress);
  getMutablePacket_().push_back(static_cast<uint8_t>(getConfig_().transactionID >> 8));
  getMutablePacket_().push_back(static_cast<uint8_t>(getConfig_().transactionID & 0xFF));
  auto crc = CRC::calcCRC(std::span(getMutablePacket_()).subspan(getConfig_().replyAddress.size()));
  getMutablePacket_().push_back(crc);
};

auto ReadReplyPacketBuilder::build() -> void {
  getMutablePacket_().clear();
  for (const auto& byte : getConfig_().replyAddress) {
    getMutablePacket_().push_back(byte);
  }
  getMutablePacket_().push_back(getConfig_().initiatorLogicalAddress);
  getMutablePacket_().push_back(RMAPProtocolIdentifier);
  {  // Instruction field
    uint8_t instruction = 0;
    instruction |= (std::to_underlying(RMAPPacketType::Reply));
    instruction |= std::to_underlying(RMAPCommandCode::Reply);
    if (getConfig_().incrementMode) {
      instruction |= std::to_underlying(RMAPCommandCode::IncrementAddress);
    }
    getMutablePacket_().push_back(instruction);
  }
  getMutablePacket_().push_back(getConfig_().status);
  getMutablePacket_().push_back(getConfig_().targetLogicalAddress);
  getMutablePacket_().push_back(static_cast<uint8_t>(getConfig_().transactionID >> 8));
  getMutablePacket_().push_back(static_cast<uint8_t>(getConfig_().transactionID & 0xFF));
  getMutablePacket_().push_back(0x00);  // Reserved byte
  auto dataLength = getConfig_().data.size();
  getMutablePacket_().push_back(static_cast<uint8_t>((dataLength >> 16) & 0xFF));
  getMutablePacket_().push_back(static_cast<uint8_t>((dataLength >> 8) & 0xFF));
  getMutablePacket_().push_back(static_cast<uint8_t>((dataLength >> 0) & 0xFF));
  auto crc = CRC::calcCRC(std::span(getMutablePacket_()).subspan(getConfig_().replyAddress.size()));
  getMutablePacket_().push_back(crc);

  // Append data
  for (const auto& byte : getConfig_().data) {
    getMutablePacket_().push_back(byte);
  }
  auto data_crc = CRC::calcCRC(std::span(getConfig_().data));
  getMutablePacket_().push_back(data_crc);
};

}  // namespace SpwRmap
