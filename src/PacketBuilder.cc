#include "SpwRmap/PacketBuilder.hh"

#include <algorithm>
#include <cassert>
#include <print>

#include "SpwRmap/CRC.hh"
#include "SpwRmap/RMAPPacketType.hh"

namespace SpwRmap {

auto ReadPacketBuilder::calcTotalSize_() const noexcept -> size_t {
  return getConfig_().targetSpaceWireAddress.size() +
         4 +  // Target SpaceWire address, target logical address, protocol ID
         ((getConfig_().replyAddress.size() + 3) / 4 * 4) +  // Reply address
         12;
}

auto ReadPacketBuilder::buildImpl() -> void {
  size_t total_size = 0;
  total_size += getConfig_().targetSpaceWireAddress.size();
  total_size += 4;
  total_size += (getConfig_().replyAddress.size() + 3) / 4 * 4;
  total_size += 12;
  if (getPacket_().size() < total_size) {
    if (usingInternalBuffer_()) {
      resizeInternalBuffer_(total_size);
    } else if (getPacket_().size() == 0) {
      reservePacket(total_size);
    } else {
      std::println(
          "Packet size is too small for RMAP read packet, "
          "expected: {}, actual: {}",
          total_size, getPacket_().size());
      throw std::runtime_error("Packet size is too small for RMAP read packet");
    }
  }
  setTotalSize_(total_size);

  std::fill(getPacket_().begin(), getPacket_().end(), 0x00);
  auto head = 0;
  for (const auto& byte : getConfig_().targetSpaceWireAddress) {
    getPacket_()[head++] = byte;
  }
  getPacket_()[head++] = (getConfig_().targetLogicalAddress);
  getPacket_()[head++] = (RMAPProtocolIdentifier);  // Protocol Identifier
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
      replyAddressSize =
          ((replyAddressSize - 1) & 0x0C) + 0x04;  // Convert to 4-byte words
      instruction |= (replyAddressSize >> 2);
    }
    getPacket_()[head++] = (instruction);
  }
  getPacket_()[head++] = (getConfig_().key);
  if (replyAddressSize != 0) {
    for (size_t i = 0; i < replyAddressSize - getConfig_().replyAddress.size();
         ++i) {
      getPacket_()[head++] = (0x00);
    }
  }
  for (const auto& byte : getConfig_().replyAddress) {
    getPacket_()[head++] = (byte);
  }
  getPacket_()[head++] = (getConfig_().initiatorLogicalAddress);
  getPacket_()[head++] =
      (static_cast<uint8_t>(getConfig_().transactionID >> 8));
  getPacket_()[head++] =
      (static_cast<uint8_t>(getConfig_().transactionID & 0xFF));
  getPacket_()[head++] = (getConfig_().extendedAddress);
  getPacket_()[head++] =
      (static_cast<uint8_t>((getConfig_().address >> 24) & 0xFF));
  getPacket_()[head++] =
      (static_cast<uint8_t>((getConfig_().address >> 16) & 0xFF));
  getPacket_()[head++] =
      (static_cast<uint8_t>((getConfig_().address >> 8) & 0xFF));
  getPacket_()[head++] =
      (static_cast<uint8_t>((getConfig_().address >> 0) & 0xFF));
  getPacket_()[head++] =
      (static_cast<uint8_t>((getConfig_().dataLength >> 16) & 0xFF));
  getPacket_()[head++] =
      (static_cast<uint8_t>((getConfig_().dataLength >> 8) & 0xFF));
  getPacket_()[head++] =
      (static_cast<uint8_t>((getConfig_().dataLength >> 0) & 0xFF));
  auto crc = CRC::calcCRC(
      std::span(getPacket_())
          .subspan(getConfig_().targetSpaceWireAddress.size(),
                   head - getConfig_().targetSpaceWireAddress.size()));
  getPacket_()[head++] = (crc);
};

auto WritePacketBuilder::calcTotalSize_() const noexcept -> size_t {
  return getConfig_().targetSpaceWireAddress.size() + 4 +
         ((getConfig_().replyAddress.size() + 3) / 4 * 4) + 12 +
         getConfig_().data.size() + 1;
}

auto WritePacketBuilder::buildImpl() -> void {
  size_t total_size = 0;
  total_size += getConfig_().targetSpaceWireAddress.size();
  total_size += 4;
  total_size += (getConfig_().replyAddress.size() + 3) / 4 * 4;
  total_size += 12;
  total_size += getConfig_().data.size() + 1;
  if (getPacket_().size() < total_size) {
    if (usingInternalBuffer_()) {
      resizeInternalBuffer_(total_size);
    } else if (getPacket_().size() == 0) {
      reservePacket(total_size);
    } else {
      throw std::runtime_error("Packet size is too small for RMAP read packet");
    }
  }
  setTotalSize_(total_size);

  std::fill(getPacket_().begin(), getPacket_().end(), 0x00);
  auto head = 0;
  for (const auto& byte : getConfig_().targetSpaceWireAddress) {
    getPacket_()[head++] = (byte);
  }
  getPacket_()[head++] = (getConfig_().targetLogicalAddress);
  getPacket_()[head++] = (RMAPProtocolIdentifier);
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
      replyAddressSize =
          ((replyAddressSize - 1) & 0x0C) + 0x04;  // Convert to 4-byte words
      instruction |= (replyAddressSize >> 2);
    }
    getPacket_()[head++] = (instruction);
  }
  getPacket_()[head++] = (getConfig_().key);
  if (replyAddressSize != 0) {
    for (size_t i = 0; i < replyAddressSize - getConfig_().replyAddress.size();
         ++i) {
      getPacket_()[head++] = (0x00);
    }
  }
  for (const auto& byte : getConfig_().replyAddress) {
    getPacket_()[head++] = (byte);
  }
  getPacket_()[head++] = (getConfig_().initiatorLogicalAddress);
  getPacket_()[head++] =
      (static_cast<uint8_t>(getConfig_().transactionID >> 8));
  getPacket_()[head++] =
      (static_cast<uint8_t>(getConfig_().transactionID & 0xFF));
  getPacket_()[head++] = (getConfig_().extendedAddress);
  getPacket_()[head++] =
      (static_cast<uint8_t>((getConfig_().address >> 24) & 0xFF));
  getPacket_()[head++] =
      (static_cast<uint8_t>((getConfig_().address >> 16) & 0xFF));
  getPacket_()[head++] =
      (static_cast<uint8_t>((getConfig_().address >> 8) & 0xFF));
  getPacket_()[head++] =
      (static_cast<uint8_t>((getConfig_().address >> 0) & 0xFF));

  auto dataLength = getConfig_().data.size();
  getPacket_()[head++] = (static_cast<uint8_t>((dataLength >> 16) & 0xFF));
  getPacket_()[head++] = (static_cast<uint8_t>((dataLength >> 8) & 0xFF));
  getPacket_()[head++] = (static_cast<uint8_t>((dataLength >> 0) & 0xFF));

  auto crc = CRC::calcCRC(
      std::span(getPacket_())
          .subspan(getConfig_().targetSpaceWireAddress.size(),
                   head - getConfig_().targetSpaceWireAddress.size()));
  getPacket_()[head++] = (crc);

  // Append data
  for (const auto& byte : getConfig_().data) {
    getPacket_()[head++] = (byte);
  }
  auto data_crc = CRC::calcCRC(std::span(getConfig_().data));
  getPacket_()[head++] = (data_crc);
};

auto WriteReplyPacketBuilder::calcTotalSize_() const noexcept -> size_t {
  return getConfig_().replyAddress.size() + 8;
}

auto WriteReplyPacketBuilder::buildImpl() -> void {
  size_t total_size = 0;
  total_size += getConfig_().replyAddress.size();
  total_size += 8;
  if (getPacket_().size() < total_size) {
    if (usingInternalBuffer_()) {
      resizeInternalBuffer_(total_size);
    } else if (getPacket_().size() == 0) {
      reservePacket(total_size);
    } else {
      throw std::runtime_error("Packet size is too small for RMAP read packet");
    }
  }
  setTotalSize_(total_size);

  std::fill(getPacket_().begin(), getPacket_().end(), 0x00);
  auto head = 0;
  for (const auto& byte : getConfig_().replyAddress) {
    getPacket_()[head++] = (byte);
  }
  getPacket_()[head++] = (getConfig_().initiatorLogicalAddress);
  getPacket_()[head++] = (0x01);  // Protocol Identifier
  {                               // Instruction field
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
    getPacket_()[head++] = (instruction);
  }
  getPacket_()[head++] = (getConfig_().status);
  getPacket_()[head++] = (getConfig_().targetLogicalAddress);
  getPacket_()[head++] =
      (static_cast<uint8_t>(getConfig_().transactionID >> 8));
  getPacket_()[head++] =
      (static_cast<uint8_t>(getConfig_().transactionID & 0xFF));
  auto crc =
      CRC::calcCRC(std::span(getPacket_())
                       .subspan(getConfig_().replyAddress.size(),
                                head - getConfig_().replyAddress.size()));
  getPacket_()[head++] = (crc);
};

auto ReadReplyPacketBuilder::calcTotalSize_() const noexcept -> size_t {
  return getConfig_().replyAddress.size() + 12 + getConfig_().data.size() + 1;
}

auto ReadReplyPacketBuilder::buildImpl() -> void {
  size_t total_size = 0;
  total_size += getConfig_().replyAddress.size();
  total_size += 12;
  total_size += getConfig_().data.size() + 1;
  if (getPacket_().size() < total_size) {
    if (usingInternalBuffer_()) {
      resizeInternalBuffer_(total_size);
    } else if (getPacket_().size() == 0) {
      reservePacket(total_size);
    } else {
      throw std::runtime_error("Packet size is too small for RMAP read packet");
    }
  }
  setTotalSize_(total_size);

  std::fill(getPacket_().begin(), getPacket_().end(), 0x00);
  auto head = 0;
  for (const auto& byte : getConfig_().replyAddress) {
    getPacket_()[head++] = (byte);
  }
  getPacket_()[head++] = (getConfig_().initiatorLogicalAddress);
  getPacket_()[head++] = (RMAPProtocolIdentifier);
  {  // Instruction field
    uint8_t instruction = 0;
    instruction |= (std::to_underlying(RMAPPacketType::Reply));
    instruction |= std::to_underlying(RMAPCommandCode::Reply);
    if (getConfig_().incrementMode) {
      instruction |= std::to_underlying(RMAPCommandCode::IncrementAddress);
    }
    getPacket_()[head++] = (instruction);
  }
  getPacket_()[head++] = (getConfig_().status);
  getPacket_()[head++] = (getConfig_().targetLogicalAddress);
  getPacket_()[head++] =
      (static_cast<uint8_t>(getConfig_().transactionID >> 8));
  getPacket_()[head++] =
      (static_cast<uint8_t>(getConfig_().transactionID & 0xFF));
  getPacket_()[head++] = (0x00);  // Reserved byte
  auto dataLength = getConfig_().data.size();
  getPacket_()[head++] = (static_cast<uint8_t>((dataLength >> 16) & 0xFF));
  getPacket_()[head++] = (static_cast<uint8_t>((dataLength >> 8) & 0xFF));
  getPacket_()[head++] = (static_cast<uint8_t>((dataLength >> 0) & 0xFF));
  auto crc =
      CRC::calcCRC(std::span(getPacket_())
                       .subspan(getConfig_().replyAddress.size(),
                                head - getConfig_().replyAddress.size()));
  getPacket_()[head++] = (crc);

  // Append data
  for (const auto& byte : getConfig_().data) {
    getPacket_()[head++] = (byte);
  }
  auto data_crc = CRC::calcCRC(std::span(getConfig_().data));
  getPacket_()[head++] = (data_crc);
};

}  // namespace SpwRmap
