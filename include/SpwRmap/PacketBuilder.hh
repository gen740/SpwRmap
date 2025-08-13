#pragma once

#include <cassert>
#include <cstdint>
#include <print>
#include <utility>
#include <vector>

#include "SpwRmap/CRC.hh"
#include "SpwRmap/RMAPPacketType.hh"

namespace SpwRmap {

struct ReadPacketConfig {
  std::span<const uint8_t> targetSpaceWireAddress;
  std::span<const uint8_t> replyAddress;
  uint8_t targetLogicalAddress{0};
  uint8_t initiatorLogicalAddress{0};
  uint16_t transactionID{0};
  uint8_t extendedAddress{0};
  uint32_t address{0};
  uint32_t dataLength{0};
  uint8_t key{0};
  bool incrementMode{true};
};

class ReadPacketBuilder {
 private:
  ReadPacketConfig config_;

  std::vector<uint8_t> packet_;

 public:
  ReadPacketBuilder(ReadPacketConfig config) noexcept : config_(std::move(config)) {}

  [[nodiscard]] auto getPacket() const noexcept -> const std::vector<uint8_t>& { return packet_; }

  [[nodiscard]] auto getPacketSize() const noexcept -> size_t { return packet_.size(); }

  auto build() -> void {
    packet_.clear();
    for (const auto& byte : config_.targetSpaceWireAddress) {
      packet_.push_back(byte);
    }
    packet_.push_back(config_.targetLogicalAddress);
    packet_.push_back(0x01);  // Protocol Identifier
    auto replyAddressSize = config_.replyAddress.size();
    {  // Instruction field
      uint8_t instruction = 0;
      instruction |= (0b01 << 6);
      instruction |=
          (std::to_underlying(RMAPPacketType::Read) | std::to_underlying(RMAPPacketType::Reply));
      if (config_.incrementMode) {
        instruction |= std::to_underlying(RMAPPacketType::IncrementAddress);
      }
      if (replyAddressSize != 0) {
        assert(replyAddressSize <= 12);
        replyAddressSize = ((replyAddressSize - 1) & 0x0C) + 0x04;  // Convert to 4-byte words
        instruction |= (replyAddressSize >> 2);
      }
      packet_.push_back(instruction);
    }
    packet_.push_back(config_.key);
    if (replyAddressSize != 0) {
      for (size_t i = 0; i < replyAddressSize - config_.replyAddress.size(); ++i) {
        packet_.push_back(0x00);
      }
    }
    for (const auto& byte : config_.replyAddress) {
      packet_.push_back(byte);
    }

    packet_.push_back(config_.initiatorLogicalAddress);
    packet_.push_back(static_cast<uint8_t>(config_.transactionID >> 8));
    packet_.push_back(static_cast<uint8_t>(config_.transactionID & 0xFF));
    packet_.push_back(config_.extendedAddress);
    packet_.push_back(static_cast<uint8_t>((config_.address >> 24) & 0xFF));
    packet_.push_back(static_cast<uint8_t>((config_.address >> 16) & 0xFF));
    packet_.push_back(static_cast<uint8_t>((config_.address >> 8) & 0xFF));
    packet_.push_back(static_cast<uint8_t>((config_.address >> 0) & 0xFF));

    // TODO: Data length should be 24bit
    packet_.push_back(static_cast<uint8_t>((config_.dataLength >> 16) & 0xFF));
    packet_.push_back(static_cast<uint8_t>((config_.dataLength >> 8) & 0xFF));
    packet_.push_back(static_cast<uint8_t>((config_.dataLength >> 0) & 0xFF));

    auto crc = calcCRC(std::span(packet_).subspan(config_.targetSpaceWireAddress.size()));
    packet_.push_back(crc);
  };
};

struct WritePacketConfig {
  std::span<const uint8_t> targetSpaceWireAddress;
  std::span<const uint8_t> replyAddress;
  uint8_t targetLogicalAddress{0};
  uint8_t initiatorLogicalAddress{0};
  uint16_t transactionID{0};
  uint8_t key{0};
  uint8_t extendedAddress{0};
  uint32_t address{0};
  bool incrementMode{true};
  bool reply{true};
  bool verifyMode{true};
  std::span<const uint8_t> data;
};

class WritePacketBuilder {
 private:
  WritePacketConfig config_;

  std::vector<uint8_t> packet_;

 public:
  WritePacketBuilder(WritePacketConfig config) noexcept : config_(std::move(config)) {}

  [[nodiscard]] auto getPacket() const noexcept -> const std::vector<uint8_t>& { return packet_; }

  [[nodiscard]] auto getPacketSize() const noexcept -> size_t { return packet_.size(); }

  auto build() -> void {
    packet_.clear();
    for (const auto& byte : config_.targetSpaceWireAddress) {
      packet_.push_back(byte);
    }
    packet_.push_back(config_.targetLogicalAddress);
    packet_.push_back(0x01);  // Protocol Identifier
    auto replyAddressSize = config_.replyAddress.size();
    {  // Instruction field
      uint8_t instruction = 0;
      instruction |= (0b01 << 6);
      instruction |= (std::to_underlying(RMAPPacketType::Write));
      if (config_.reply) {
        instruction |= std::to_underlying(RMAPPacketType::Reply);
      }
      if (config_.verifyMode) {
        instruction |= std::to_underlying(RMAPPacketType::VerifyDataBeforeWrite);
      }
      if (config_.incrementMode) {
        instruction |= std::to_underlying(RMAPPacketType::IncrementAddress);
      }
      if (replyAddressSize != 0) {
        assert(replyAddressSize <= 12);
        replyAddressSize = ((replyAddressSize - 1) & 0x0C) + 0x04;  // Convert to 4-byte words
        instruction |= (replyAddressSize >> 2);
      }
      packet_.push_back(instruction);
    }
    packet_.push_back(config_.key);
    if (replyAddressSize != 0) {
      for (size_t i = 0; i < replyAddressSize - config_.replyAddress.size(); ++i) {
        packet_.push_back(0x00);
      }
    }
    for (const auto& byte : config_.replyAddress) {
      packet_.push_back(byte);
    }

    packet_.push_back(config_.initiatorLogicalAddress);
    packet_.push_back(static_cast<uint8_t>(config_.transactionID >> 8));
    packet_.push_back(static_cast<uint8_t>(config_.transactionID & 0xFF));
    packet_.push_back(config_.extendedAddress);
    packet_.push_back(static_cast<uint8_t>((config_.address >> 24) & 0xFF));
    packet_.push_back(static_cast<uint8_t>((config_.address >> 16) & 0xFF));
    packet_.push_back(static_cast<uint8_t>((config_.address >> 8) & 0xFF));
    packet_.push_back(static_cast<uint8_t>((config_.address >> 0) & 0xFF));

    auto dataLength = config_.data.size();

    // TODO: Data length should be 24bit
    packet_.push_back(static_cast<uint8_t>((dataLength >> 16) & 0xFF));
    packet_.push_back(static_cast<uint8_t>((dataLength >> 8) & 0xFF));
    packet_.push_back(static_cast<uint8_t>((dataLength >> 0) & 0xFF));

    auto crc = calcCRC(std::span(packet_).subspan(config_.targetSpaceWireAddress.size()));
    packet_.push_back(crc);

    for (const auto& byte : config_.data) {
      packet_.push_back(byte);
    }
    auto data_crc = calcCRC(std::span(config_.data));
    packet_.push_back(data_crc);
  };
};

};  // namespace SpwRmap
