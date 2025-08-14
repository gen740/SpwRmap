#pragma once

#include <cassert>
#include <cstdint>
#include <print>
#include <utility>
#include <vector>

#include "SpwRmap/CRC.hh"
#include "SpwRmap/RMAPPacketType.hh"

namespace SpwRmap {

template <class ConfigT>
class PacketBuilderBase {
 private:
  ConfigT config_{};
  std::vector<uint8_t> packet_;

 protected:
  [[nodiscard]] auto getConfig_() const noexcept -> const ConfigT& { return config_; }
  [[nodiscard]] auto getMutablePacket_() noexcept -> std::vector<uint8_t>& { return packet_; }

 public:
  explicit PacketBuilderBase() noexcept = default;
  explicit PacketBuilderBase(ConfigT config) noexcept : config_(std::move(config)) {}
  auto setConfig(ConfigT config) noexcept { config_ = std::move(config); }
  auto getMutableConfig() noexcept -> ConfigT& { return config_; }

  [[nodiscard]] auto getPacket() const noexcept -> const std::vector<uint8_t>& { return packet_; }

  PacketBuilderBase(const PacketBuilderBase&) = delete;
  auto operator=(const PacketBuilderBase&) -> PacketBuilderBase& = delete;
  PacketBuilderBase(PacketBuilderBase&&) = delete;
  auto operator=(PacketBuilderBase&&) -> PacketBuilderBase& = delete;

  virtual auto build() -> void = 0;
};

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

struct ReadReplyPacketConfig {
  std::span<const uint8_t> replyAddress;
  uint8_t initiatorLogicalAddress{0};
  uint8_t status{0};
  uint8_t targetLogicalAddress{0};
  uint16_t transactionID{0};
  std::span<const uint8_t> data;
  bool incrementMode{true};
};

struct WriteReplyPacketConfig {
  std::span<const uint8_t> replyAddress;
  uint8_t initiatorLogicalAddress{0};
  uint8_t status{0};
  uint8_t targetLogicalAddress{0};
  uint16_t transactionID{0};
  bool incrementMode{true};
  bool verifyMode{true};
};

class ReadPacketBuilder final : public PacketBuilderBase<ReadPacketConfig> {
 public:
  using PacketBuilderBase<ReadPacketConfig>::PacketBuilderBase;
  auto build() -> void override {
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
    auto crc =
        calcCRC(std::span(getMutablePacket_()).subspan(getConfig_().targetSpaceWireAddress.size()));
    getMutablePacket_().push_back(crc);
  };
};

class WritePacketBuilder final : public PacketBuilderBase<WritePacketConfig> {
 public:
  using PacketBuilderBase<WritePacketConfig>::PacketBuilderBase;
  auto build() -> void override {
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

    auto crc =
        calcCRC(std::span(getMutablePacket_()).subspan(getConfig_().targetSpaceWireAddress.size()));
    getMutablePacket_().push_back(crc);

    // Append data
    for (const auto& byte : getConfig_().data) {
      getMutablePacket_().push_back(byte);
    }
    auto data_crc = calcCRC(std::span(getConfig_().data));
    getMutablePacket_().push_back(data_crc);
  };
};

class WriteReplyPacketBuilder final : public PacketBuilderBase<WriteReplyPacketConfig> {
 public:
  using PacketBuilderBase<WriteReplyPacketConfig>::PacketBuilderBase;
  auto build() -> void override {
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
    auto crc = calcCRC(std::span(getMutablePacket_()).subspan(getConfig_().replyAddress.size()));
    getMutablePacket_().push_back(crc);
  };
};

class ReadReplyPacketBuilder final : public PacketBuilderBase<ReadReplyPacketConfig> {
 public:
  using PacketBuilderBase<ReadReplyPacketConfig>::PacketBuilderBase;

  auto build() -> void override {
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
    auto crc = calcCRC(std::span(getMutablePacket_()).subspan(getConfig_().replyAddress.size()));
    getMutablePacket_().push_back(crc);

    // Append data
    for (const auto& byte : getConfig_().data) {
      getMutablePacket_().push_back(byte);
    }
    auto data_crc = calcCRC(std::span(getConfig_().data));
    getMutablePacket_().push_back(data_crc);
  };
};

};  // namespace SpwRmap
