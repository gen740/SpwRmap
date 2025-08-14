#pragma once

#include <print>
#include <span>
#include <vector>

namespace SpwRmap {

enum class PacketType {
  Undefined = 0,
  Read = 1,
  Write = 2,
  ReadReply = 3,
  WriteReply = 4,
};

struct Packet {
  std::span<const uint8_t> targetSpaceWireAddress{};
  std::span<const uint8_t> replyAddress{};
  uint8_t initiatorLogicalAddress{};
  uint8_t instruction{};
  uint8_t key{};
  uint8_t status{};
  uint8_t targetLogicalAddress{};
  uint16_t transactionID{};
  uint8_t extendedAddress{};
  uint32_t address{};
  uint32_t dataLength{};
  std::span<const uint8_t> data{};
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

  [[nodiscard]] auto parseReadPacket(const std::span<const uint8_t> packet) noexcept -> StatusCode;

  [[nodiscard]] auto parseReadReplyPacket(const std::span<const uint8_t> packet) noexcept
      -> StatusCode;

  [[nodiscard]] auto parseWritePacket(const std::span<const uint8_t> packet) noexcept -> StatusCode;

  [[nodiscard]] auto parseWriteReplyPacket(const std::span<const uint8_t> packet) noexcept
      -> StatusCode;

  [[nodiscard]] auto parse(const std::span<const uint8_t> packet) noexcept -> StatusCode;

  [[nodiscard]] auto getPacket() const noexcept -> const Packet& { return packet_; }
};

};  // namespace SpwRmap
