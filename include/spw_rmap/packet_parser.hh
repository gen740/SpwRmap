// Copyright (c) 2025 Gen
// Licensed under the MIT License. See LICENSE file for details.
#pragma once

#include <cstdint>
#include <span>
#include <system_error>

namespace spw_rmap {

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
  enum class Status {
    Success = 0,
    InvalidPacket = 2,
    HeaderCRCError = 3,
    DataCRCError = 4,
    IncompletePacket = 5,
    NotReplyPacket = 6,
    PacketStatusError = 7,
    UnknownProtocolIdentifier = 8,
  };

  [[nodiscard]] auto parseReadPacket(
      const std::span<const uint8_t> packet) noexcept -> Status;

  [[nodiscard]] auto parseReadReplyPacket(
      const std::span<const uint8_t> packet) noexcept -> Status;

  [[nodiscard]] auto parseWritePacket(
      const std::span<const uint8_t> packet) noexcept -> Status;

  [[nodiscard]] auto parseWriteReplyPacket(
      const std::span<const uint8_t> packet) noexcept -> Status;

  [[nodiscard]] auto parse(const std::span<const uint8_t> packet) noexcept
      -> Status;

  [[nodiscard]] auto getPacket() const noexcept -> const Packet& {
    return packet_;
  }
};

class StatusCodeCategory final : public std::error_category {
 public:
  [[nodiscard]] auto name() const noexcept -> const char* override {
    return "StatusCode";
  }

  [[nodiscard]] auto message(int ev) const -> std::string override {
    switch (static_cast<PacketParser::Status>(ev)) {
      case PacketParser::Status::Success:
        return "Success";
      case PacketParser::Status::InvalidPacket:
        return "Invalid packet";
      case PacketParser::Status::HeaderCRCError:
        return "Header CRC error";
      case PacketParser::Status::DataCRCError:
        return "Data CRC error";
      case PacketParser::Status::IncompletePacket:
        return "Incomplete packet";
      case PacketParser::Status::NotReplyPacket:
        return "Not a reply packet";
      case PacketParser::Status::PacketStatusError:
        return "Packet status error";
      case PacketParser::Status::UnknownProtocolIdentifier:
        return "Unknown protocol identifier";
      default:
        return "Unknown status code";
    }
  }
};

inline auto status_code_category() noexcept -> const std::error_category& {
  static StatusCodeCategory instance;
  return instance;
}

inline auto make_error_code(PacketParser::Status e) noexcept
    -> std::error_code {
  return {static_cast<int>(e), status_code_category()};
}

};  // namespace spw_rmap

namespace std {
template <>
struct is_error_code_enum<spw_rmap::PacketParser::Status> : true_type {};

}  // namespace std
