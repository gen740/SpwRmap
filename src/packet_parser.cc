#include "spw_rmap/packet_parser.hh"

#include <utility>
#include <iostream>

#include "spw_rmap/crc.hh"
#include "spw_rmap/rmap_packet_type.hh"

namespace spw_rmap {

auto PacketParser::parseReadPacket(
    const std::span<const uint8_t> packet) noexcept -> Status {
  size_t head = 0;
  size_t replyAddressSize =
      static_cast<size_t>(packet_.instruction & 0b00000011) * 4;
  if (packet.size() != 16 + replyAddressSize) {
    return Status::IncompletePacket;
  }
  if (crc::calcCRC(packet.subspan(0, 16 + replyAddressSize)) != 0x00) {
    return Status::HeaderCRCError;
  }
  packet_.targetLogicalAddress = packet[head++];
  if (packet[head++] != 0x01) {
    return Status::UnknownProtocolIdentifier;
  }
  packet_.instruction = packet[head++];
  packet_.key = packet[head++];

  auto replyAddressFirstByte = head;
  auto replyAddressActualSize = replyAddressSize;
  for (size_t i = 0; i < replyAddressSize; ++i) {
    if (packet[head++] == 0x00) {
      replyAddressFirstByte = head;
      replyAddressActualSize--;
    } else {
      head += replyAddressActualSize - 1;
      break;
    }
  }
  packet_.replyAddress =
      packet.subspan(replyAddressFirstByte, replyAddressActualSize);

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
  return Status::Success;
}

auto PacketParser::parseReadReplyPacket(
    const std::span<const uint8_t> packet) noexcept -> Status {
  size_t head = 0;
  if (packet.size() < 12) {
    return Status::IncompletePacket;
  }
  if (crc::calcCRC(packet.subspan(0, 12)) != 0x00) {
    return Status::HeaderCRCError;
  }
  packet_.initiatorLogicalAddress = packet[head++];
  if (packet[head++] != 0x01) {
    return Status::UnknownProtocolIdentifier;
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
    return Status::IncompletePacket;
  }
  if (crc::calcCRC(packet.subspan(12, packet_.dataLength + 1)) != 0x00) {
    return Status::DataCRCError;
  }
  head++;
  packet_.data =
      std::span<const uint8_t>(packet).subspan(head, packet_.dataLength);
  return Status::Success;
}
auto PacketParser::parseWritePacket(
    const std::span<const uint8_t> packet) noexcept -> Status {
  size_t head = 0;
  size_t replyAddressSize =
      static_cast<size_t>(packet_.instruction & 0b00000011) * 4;
  if (packet.size() <= 16 + replyAddressSize) {
    return Status::IncompletePacket;
  }
  if (crc::calcCRC(packet.subspan(0, 16 + replyAddressSize)) != 0x00) {
    return Status::HeaderCRCError;
  }
  packet_.targetLogicalAddress = packet[head++];
  if (packet[head++] != 0x01) {
    return Status::UnknownProtocolIdentifier;
  }
  packet_.instruction = packet[head++];
  packet_.key = packet[head++];
  auto replyAddressFirstByte = head;
  auto replyAddressActualSize = replyAddressSize;
  for (size_t i = 0; i < replyAddressSize; ++i) {
    if (packet[head++] == 0x00) {
      replyAddressFirstByte = head;
      replyAddressActualSize--;
    } else {
      head += replyAddressActualSize - 1;
      break;
    }
  }
  packet_.replyAddress =
      packet.subspan(replyAddressFirstByte, replyAddressActualSize);
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
    return Status::IncompletePacket;
  }
  if (crc::calcCRC(packet.subspan(16 + replyAddressSize,
                                  packet_.dataLength + 1)) != 0x00) {
    return Status::DataCRCError;
  }
  head++;  // Skip CRC byte
  packet_.data =
      std::span<const uint8_t>(packet).subspan(head, packet_.dataLength);
  return Status::Success;
}
auto PacketParser::parseWriteReplyPacket(
    const std::span<const uint8_t> packet) noexcept -> Status {
  size_t head = 0;
  if (packet.size() != 8) {
    return Status::IncompletePacket;
  }
  if (crc::calcCRC(packet.subspan(0, 8)) != 0x00) {
    return Status::HeaderCRCError;
  }
  packet_.initiatorLogicalAddress = packet[head++];
  if (packet[head++] != 0x01) {
    return Status::UnknownProtocolIdentifier;
  }
  packet_.instruction = packet[head++];
  packet_.status = packet[head++];
  packet_.targetLogicalAddress = packet[head++];
  packet_.transactionID = 0;
  packet_.transactionID |= (packet[head++] << 8);
  packet_.transactionID |= (packet[head++] << 0);
  return Status::Success;
}
auto PacketParser::parse(const std::span<const uint8_t> packet) noexcept
    -> Status {
  size_t head = 0;

  // Parse target SpaceWire address
  while (packet[head] < 0x20) {
    head++;
    if (head >= packet.size()) [[unlikely]] {
      return Status::IncompletePacket;
    }
  }

  // Check size
  if (packet.size() - head < 4) {
    return Status::IncompletePacket;
  }
  packet_.instruction = packet[head + 2];

  bool is_command = (packet_.instruction & 0b01000000) != 0;
  bool is_write =
      (packet_.instruction & std::to_underlying(RMAPCommandCode::Write)) != 0;

  switch (is_command << 1 | is_write) {
    case 0b00:  // Read reply
      packet_.type = PacketType::ReadReply;
      packet_.replyAddress = std::span<const uint8_t>(packet).subspan(0, head);
      return parseReadReplyPacket(packet.subspan(head));
    case 0b01:  // Write reply
      packet_.type = PacketType::WriteReply;
      packet_.replyAddress = std::span<const uint8_t>(packet).subspan(0, head);
      return parseWriteReplyPacket(packet.subspan(head));
    case 0b10:  // Read command
      packet_.type = PacketType::Read;
      packet_.targetSpaceWireAddress =
          std::span<const uint8_t>(packet).subspan(0, head);
      return parseReadPacket(packet.subspan(head));
    case 0b11:  // Write command
      packet_.type = PacketType::Write;
      packet_.targetSpaceWireAddress =
          std::span<const uint8_t>(packet).subspan(0, head);
      return parseWritePacket(packet.subspan(head));
    default:
      std::unreachable();
  }
}

}  // namespace spw_rmap
