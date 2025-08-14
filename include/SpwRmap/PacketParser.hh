
#pragma oncew

#include <print>
#include <utility>
#include <vector>

#include "SpwRmap/CRC.hh"
#include "SpwRmap/RMAPPacketType.hh"

namespace SpwRmap {

enum class PacketType {
  Undefined = 0,
  Read = 1,
  Write = 2,
  ReadReply = 3,
  WriteReply = 4,
};

struct Packet {
  std::vector<uint8_t> targetSpaceWireAddress{};
  std::vector<uint8_t> replyAddress{};
  uint8_t initiatorLogicalAddress{};
  uint8_t instruction{};
  uint8_t key{};
  uint8_t status{};
  uint8_t targetLogicalAddress{};
  uint16_t transactionID{};
  uint8_t extendedAddress{};
  uint32_t address{};
  uint32_t dataLength{};
  std::vector<uint8_t> data{};
  PacketType type{PacketType::Undefined};
};

enum class PacketStatusCode {
  CommandExecutedSuccessfully = 0,
  GeneralErrorCode = 1,
  UnusedRMAPPacketTypeOrCommandCode = 2,
  InvalidKey = 3,
  InvalidDataCRC = 4,
  EarlyEOP = 5,
  TooMuchData = 6,
  EEP = 7,
  VerifyBufferOverrun = 9,
  RMAPCommandNotImplementedOrNotAuthorised = 10,
  RMWDataLengthError = 11,
  InvalidTargetLogicalAddress = 12,
};

class PacketParser {
 private:
  Packet packet_{};

 public:
  enum class StatusCode {
    Success = 0,
    InvalidPacket = 2,
    HeaderCRCError = 3,
    DataCRCError = 4,
    IncompletePacket = 5,
    NotReplyPacket = 6,
    PacketStatusError = 7,
    UnknownProtocolIdentifier = 8,
  };

  [[nodiscard]] auto parseReadPacket(const std::span<const uint8_t> packet) noexcept -> StatusCode {
    size_t head = 0;
    size_t replyAddressSize = static_cast<size_t>(packet_.instruction & 0b00000011) * 4;
    if (packet.size() != 16 + replyAddressSize) {
      return StatusCode::IncompletePacket;
    }
    if (calcCRC(packet.subspan(0, 16 + replyAddressSize)) != 0x00) {
      return StatusCode::HeaderCRCError;
    }
    packet_.targetLogicalAddress = packet[head++];
    if (packet[head++] != 0x01) {
      return StatusCode::UnknownProtocolIdentifier;
    }
    packet_.instruction = packet[head++];
    packet_.key = packet[head++];
    packet_.replyAddress.clear();
    packet_.replyAddress.reserve(replyAddressSize);
    for (size_t i = 0; i < replyAddressSize; ++i) {
      auto addr = packet[head++];
      if (addr != 0x00) {
        packet_.replyAddress.push_back(addr);
      }
    }
    packet_.initiatorLogicalAddress = packet[head++];
    packet_.transactionID = 0;
    packet_.transactionID |= (packet[head++] << 8);
    packet_.transactionID |= (packet[head++] << 0);
    packet_.extendedAddress = packet[head++];
    packet_.address = 0;
    packet_.address |= (packet[head++] << 24);
    packet_.address |= (packet[head++] << 16);
    packet_.address |= (packet[head++] << 8);
    packet_.address |= (packet[head++] << 0);
    packet_.dataLength = 0;
    packet_.dataLength |= (packet[head++] << 16);
    packet_.dataLength |= (packet[head++] << 8);
    packet_.dataLength |= (packet[head++] << 0);
    return StatusCode::Success;
  }

  [[nodiscard]] auto parseReadReplyPacket(const std::span<const uint8_t> packet) noexcept
      -> StatusCode {
    size_t head = 0;
    if (packet.size() < 12) {
      return StatusCode::IncompletePacket;
    }
    if (calcCRC(packet.subspan(0, 12)) != 0x00) {
      return StatusCode::HeaderCRCError;
    }
    packet_.initiatorLogicalAddress = packet[head++];
    if (packet[head++] != 0x01) {
      return StatusCode::UnknownProtocolIdentifier;
    }
    packet_.instruction = packet[head++];
    packet_.status = packet[head++];
    packet_.targetLogicalAddress = packet[head++];
    packet_.transactionID = 0;
    packet_.transactionID |= (packet[head++] << 8);
    packet_.transactionID |= (packet[head++] << 0);
    head++;  // Skip reserved byte
    packet_.dataLength = 0;
    packet_.dataLength |= (packet[head++] << 16);
    packet_.dataLength |= (packet[head++] << 8);
    packet_.dataLength |= (packet[head++] << 0);
    if (packet.size() != 12 + packet_.dataLength + 1) {
      return StatusCode::IncompletePacket;
    }
    if (calcCRC(packet.subspan(12, packet_.dataLength + 1)) != 0x00) {
      return StatusCode::DataCRCError;
    }
    head++;  // Skip CRC byte
    packet_.data.clear();
    packet_.data.reserve(packet_.dataLength);
    for (size_t i = 0; i < packet_.dataLength; ++i) {
      packet_.data.push_back(packet[head++]);
    }
    return StatusCode::Success;
  }

  [[nodiscard]] auto parseWritePacket(const std::span<const uint8_t> packet) noexcept
      -> StatusCode {
    size_t head = 0;
    size_t replyAddressSize = static_cast<size_t>(packet_.instruction & 0b00000011) * 4;
    if (packet.size() <= 16 + replyAddressSize) {
      return StatusCode::IncompletePacket;
    }
    if (calcCRC(packet.subspan(0, 16 + replyAddressSize)) != 0x00) {
      return StatusCode::HeaderCRCError;
    }
    packet_.targetLogicalAddress = packet[head++];
    if (packet[head++] != 0x01) {
      return StatusCode::UnknownProtocolIdentifier;
    }
    packet_.instruction = packet[head++];
    packet_.key = packet[head++];
    packet_.replyAddress.clear();
    packet_.replyAddress.reserve(replyAddressSize);
    for (size_t i = 0; i < replyAddressSize; ++i) {
      auto addr = packet[head++];
      if (addr != 0x00) {
        packet_.replyAddress.push_back(addr);
      }
    }
    packet_.initiatorLogicalAddress = packet[head++];
    packet_.transactionID = 0;
    packet_.transactionID |= (packet[head++] << 8);
    packet_.transactionID |= (packet[head++] << 0);
    packet_.extendedAddress = packet[head++];
    packet_.address = 0;
    packet_.address |= (packet[head++] << 24);
    packet_.address |= (packet[head++] << 16);
    packet_.address |= (packet[head++] << 8);
    packet_.address |= (packet[head++] << 0);
    packet_.dataLength = 0;
    packet_.dataLength |= (packet[head++] << 16);
    packet_.dataLength |= (packet[head++] << 8);
    packet_.dataLength |= (packet[head++] << 0);
    if (packet.size() != 16 + replyAddressSize + packet_.dataLength + 1) {
      return StatusCode::IncompletePacket;
    }
    if (calcCRC(packet.subspan(16 + replyAddressSize, packet_.dataLength + 1)) != 0x00) {
      return StatusCode::DataCRCError;
    }
    head++;  // Skip CRC byte
    packet_.data.clear();
    packet_.data.reserve(packet_.dataLength);
    for (size_t i = 0; i < packet_.dataLength; ++i) {
      packet_.data.push_back(packet[head++]);
    }
    return StatusCode::Success;
  }

  [[nodiscard]] auto parseWriteReplyPacket(const std::span<const uint8_t> packet) noexcept
      -> StatusCode {
    size_t head = 0;
    if (packet.size() != 8) {
      return StatusCode::IncompletePacket;
    }
    if (calcCRC(packet.subspan(0, 8)) != 0x00) {
      return StatusCode::HeaderCRCError;
    }
    packet_.initiatorLogicalAddress = packet[head++];
    if (packet[head++] != 0x01) {
      return StatusCode::UnknownProtocolIdentifier;
    }
    packet_.instruction = packet[head++];
    packet_.status = packet[head++];
    packet_.targetLogicalAddress = packet[head++];
    packet_.transactionID = 0;
    packet_.transactionID |= (packet[head++] << 8);
    packet_.transactionID |= (packet[head++] << 0);
    return StatusCode::Success;
  }

  [[nodiscard]] auto parse(const std::span<const uint8_t> packet) noexcept -> StatusCode {
    size_t head = 0;

    // Parse target SpaceWire address
    while (packet[head] < 0x20) {
      packet_.targetSpaceWireAddress.push_back(packet[head++]);
      if (head >= packet.size()) [[unlikely]] {
        return StatusCode::IncompletePacket;
      }
    }

    // Check size
    if (packet.size() - head < 4) {
      return StatusCode::IncompletePacket;
    }
    packet_.instruction = packet[head + 2];

    bool is_command = (packet_.instruction & 0b01000000) != 0;
    bool is_write = (packet_.instruction & std::to_underlying(RMAPCommandCode::Write)) != 0;

    switch (is_command << 1 | is_write) {
      case 0b00:  // Read reply
        packet_.type = PacketType::ReadReply;
        return parseReadReplyPacket(packet.subspan(head));
      case 0b01:  // Write reply
        packet_.type = PacketType::WriteReply;
        return parseWriteReplyPacket(packet.subspan(head));
      case 0b10:  // Read command
        packet_.type = PacketType::Read;
        return parseReadPacket(packet.subspan(head));
      case 0b11:  // Write command
        packet_.type = PacketType::Write;
        return parseWritePacket(packet.subspan(head));
      default:
        std::unreachable();
    }
  }

  [[nodiscard]] auto getPacket() const noexcept -> const Packet& { return packet_; }
};

};  // namespace SpwRmap
