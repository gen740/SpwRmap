#include "SpwRmap/PacketBuilder.hh"

#include <cassert>
#include <print>

#include "SpwRmap/CRC.hh"
#include "SpwRmap/RMAPPacketType.hh"

namespace SpwRmap {

auto ReadPacketBuilder::calcTotalSize_(
    const ReadPacketConfig& config) const noexcept -> size_t {
  return config.targetSpaceWireAddress.size() +
         4 +  // Target SpaceWire address, target logical address, protocol ID
         ((config.replyAddress.size() + 3) / 4 * 4) +  // Reply address
         12;
}

auto ReadPacketBuilder::buildImpl(const ReadPacketConfig& config) noexcept
    -> void {
  auto head = 0;
  for (const auto& byte : config.targetSpaceWireAddress) {
    getPacket_()[head++] = byte;
  }
  getPacket_()[head++] = (config.targetLogicalAddress);
  getPacket_()[head++] = (RMAPProtocolIdentifier);  // Protocol Identifier
  auto replyAddressSize = config.replyAddress.size();
  {  // Instruction field
    uint8_t instruction = 0;
    instruction |= std::to_underlying(RMAPPacketType::Command);
    instruction |= std::to_underlying(RMAPCommandCode::Reply);
    if (config.incrementMode) {
      instruction |= std::to_underlying(RMAPCommandCode::IncrementAddress);
    }
    if (replyAddressSize != 0) {
      assert(replyAddressSize <= 12);
      replyAddressSize =
          ((replyAddressSize - 1) & 0x0C) + 0x04;  // Convert to 4-byte words
      instruction |= (replyAddressSize >> 2);
    }
    getPacket_()[head++] = (instruction);
  }
  getPacket_()[head++] = (config.key);
  if (replyAddressSize != 0) {
    for (size_t i = 0; i < replyAddressSize - config.replyAddress.size(); ++i) {
      getPacket_()[head++] = (0x00);
    }
  }
  for (const auto& byte : config.replyAddress) {
    getPacket_()[head++] = (byte);
  }
  getPacket_()[head++] = (config.initiatorLogicalAddress);
  getPacket_()[head++] = (static_cast<uint8_t>(config.transactionID >> 8));
  getPacket_()[head++] = (static_cast<uint8_t>(config.transactionID & 0xFF));
  getPacket_()[head++] = (config.extendedAddress);
  getPacket_()[head++] = (static_cast<uint8_t>((config.address >> 24) & 0xFF));
  getPacket_()[head++] = (static_cast<uint8_t>((config.address >> 16) & 0xFF));
  getPacket_()[head++] = (static_cast<uint8_t>((config.address >> 8) & 0xFF));
  getPacket_()[head++] = (static_cast<uint8_t>((config.address >> 0) & 0xFF));
  getPacket_()[head++] =
      (static_cast<uint8_t>((config.dataLength >> 16) & 0xFF));
  getPacket_()[head++] =
      (static_cast<uint8_t>((config.dataLength >> 8) & 0xFF));
  getPacket_()[head++] =
      (static_cast<uint8_t>((config.dataLength >> 0) & 0xFF));
  auto crc =
      CRC::calcCRC(std::span(getPacket_())
                       .subspan(config.targetSpaceWireAddress.size(),
                                head - config.targetSpaceWireAddress.size()));
  getPacket_()[head++] = (crc);
};

auto WritePacketBuilder::calcTotalSize_(
    const WritePacketConfig& config) const noexcept -> size_t {
  return config.targetSpaceWireAddress.size() + 4 +
         ((config.replyAddress.size() + 3) / 4 * 4) + 12 + config.data.size() +
         1;
}

auto WritePacketBuilder::buildImpl(const WritePacketConfig& config) noexcept
    -> void {
  auto head = 0;
  for (const auto& byte : config.targetSpaceWireAddress) {
    getPacket_()[head++] = (byte);
  }
  getPacket_()[head++] = (config.targetLogicalAddress);
  getPacket_()[head++] = (RMAPProtocolIdentifier);
  auto replyAddressSize = config.replyAddress.size();
  {  // Instruction field
    uint8_t instruction = 0;
    instruction |= std::to_underlying(RMAPPacketType::Command);
    instruction |= (std::to_underlying(RMAPCommandCode::Write));
    if (config.reply) {
      instruction |= std::to_underlying(RMAPCommandCode::Reply);
    }
    if (config.verifyMode) {
      instruction |= std::to_underlying(RMAPCommandCode::VerifyDataBeforeWrite);
    }
    if (config.incrementMode) {
      instruction |= std::to_underlying(RMAPCommandCode::IncrementAddress);
    }
    if (replyAddressSize != 0) {
      assert(replyAddressSize <= 12);
      replyAddressSize =
          ((replyAddressSize - 1) & 0x0C) + 0x04;  // Convert to 4-byte words
      instruction |= (replyAddressSize >> 2);
    }
    getPacket_()[head++] = (instruction);
  }
  getPacket_()[head++] = (config.key);
  if (replyAddressSize != 0) {
    for (size_t i = 0; i < replyAddressSize - config.replyAddress.size(); ++i) {
      getPacket_()[head++] = (0x00);
    }
  }
  for (const auto& byte : config.replyAddress) {
    getPacket_()[head++] = (byte);
  }
  getPacket_()[head++] = (config.initiatorLogicalAddress);
  getPacket_()[head++] = (static_cast<uint8_t>(config.transactionID >> 8));
  getPacket_()[head++] = (static_cast<uint8_t>(config.transactionID & 0xFF));
  getPacket_()[head++] = (config.extendedAddress);
  getPacket_()[head++] = (static_cast<uint8_t>((config.address >> 24) & 0xFF));
  getPacket_()[head++] = (static_cast<uint8_t>((config.address >> 16) & 0xFF));
  getPacket_()[head++] = (static_cast<uint8_t>((config.address >> 8) & 0xFF));
  getPacket_()[head++] = (static_cast<uint8_t>((config.address >> 0) & 0xFF));

  auto dataLength = config.data.size();
  getPacket_()[head++] = (static_cast<uint8_t>((dataLength >> 16) & 0xFF));
  getPacket_()[head++] = (static_cast<uint8_t>((dataLength >> 8) & 0xFF));
  getPacket_()[head++] = (static_cast<uint8_t>((dataLength >> 0) & 0xFF));

  auto crc =
      CRC::calcCRC(std::span(getPacket_())
                       .subspan(config.targetSpaceWireAddress.size(),
                                head - config.targetSpaceWireAddress.size()));
  getPacket_()[head++] = (crc);

  // Append data
  for (const auto& byte : config.data) {
    getPacket_()[head++] = (byte);
  }
  auto data_crc = CRC::calcCRC(std::span(config.data));
  getPacket_()[head++] = (data_crc);
};

auto WriteReplyPacketBuilder::calcTotalSize_(
    const WriteReplyPacketConfig& config) const noexcept -> size_t {
  return config.replyAddress.size() + 8;
}

auto WriteReplyPacketBuilder::buildImpl(
    const WriteReplyPacketConfig& config) noexcept -> void {
  auto head = 0;
  for (const auto& byte : config.replyAddress) {
    getPacket_()[head++] = (byte);
  }
  getPacket_()[head++] = (config.initiatorLogicalAddress);
  getPacket_()[head++] = (0x01);  // Protocol Identifier
  {                               // Instruction field
    uint8_t instruction = 0;
    instruction |= (std::to_underlying(RMAPPacketType::Reply));
    instruction |= (std::to_underlying(RMAPCommandCode::Write));
    instruction |= std::to_underlying(RMAPCommandCode::Reply);
    if (config.verifyMode) {
      instruction |= std::to_underlying(RMAPCommandCode::VerifyDataBeforeWrite);
    }
    if (config.incrementMode) {
      instruction |= std::to_underlying(RMAPCommandCode::IncrementAddress);
    }
    getPacket_()[head++] = (instruction);
  }
  getPacket_()[head++] = (config.status);
  getPacket_()[head++] = (config.targetLogicalAddress);
  getPacket_()[head++] = (static_cast<uint8_t>(config.transactionID >> 8));
  getPacket_()[head++] = (static_cast<uint8_t>(config.transactionID & 0xFF));
  auto crc = CRC::calcCRC(std::span(getPacket_())
                              .subspan(config.replyAddress.size(),
                                       head - config.replyAddress.size()));
  getPacket_()[head++] = (crc);
};

auto ReadReplyPacketBuilder::calcTotalSize_(
    const ReadReplyPacketConfig& config) const noexcept -> size_t {
  return config.replyAddress.size() + 12 + config.data.size() + 1;
}

auto ReadReplyPacketBuilder::buildImpl(
    const ReadReplyPacketConfig& config) noexcept -> void {
  auto head = 0;
  for (const auto& byte : config.replyAddress) {
    getPacket_()[head++] = (byte);
  }
  getPacket_()[head++] = (config.initiatorLogicalAddress);
  getPacket_()[head++] = (RMAPProtocolIdentifier);
  {  // Instruction field
    uint8_t instruction = 0;
    instruction |= (std::to_underlying(RMAPPacketType::Reply));
    instruction |= std::to_underlying(RMAPCommandCode::Reply);
    if (config.incrementMode) {
      instruction |= std::to_underlying(RMAPCommandCode::IncrementAddress);
    }
    getPacket_()[head++] = (instruction);
  }
  getPacket_()[head++] = (config.status);
  getPacket_()[head++] = (config.targetLogicalAddress);
  getPacket_()[head++] = (static_cast<uint8_t>(config.transactionID >> 8));
  getPacket_()[head++] = (static_cast<uint8_t>(config.transactionID & 0xFF));
  getPacket_()[head++] = (0x00);  // Reserved byte
  auto dataLength = config.data.size();
  getPacket_()[head++] = (static_cast<uint8_t>((dataLength >> 16) & 0xFF));
  getPacket_()[head++] = (static_cast<uint8_t>((dataLength >> 8) & 0xFF));
  getPacket_()[head++] = (static_cast<uint8_t>((dataLength >> 0) & 0xFF));
  auto crc = CRC::calcCRC(std::span(getPacket_())
                              .subspan(config.replyAddress.size(),
                                       head - config.replyAddress.size()));
  getPacket_()[head++] = (crc);

  // Append data
  for (const auto& byte : config.data) {
    getPacket_()[head++] = (byte);
  }
  auto data_crc = CRC::calcCRC(std::span(config.data));
  getPacket_()[head++] = (data_crc);
};

}  // namespace SpwRmap
