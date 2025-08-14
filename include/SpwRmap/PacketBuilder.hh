#pragma once

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

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
  [[nodiscard]] auto getConfig() const noexcept -> const ConfigT& { return config_; }
  auto reservePacket(size_t size) -> void {
    packet_.clear();
    packet_.reserve(size);
  }
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
  auto build() -> void override;
};

class WritePacketBuilder final : public PacketBuilderBase<WritePacketConfig> {
 public:
  using PacketBuilderBase<WritePacketConfig>::PacketBuilderBase;
  auto build() -> void override;
};

class WriteReplyPacketBuilder final : public PacketBuilderBase<WriteReplyPacketConfig> {
 public:
  using PacketBuilderBase<WriteReplyPacketConfig>::PacketBuilderBase;
  auto build() -> void override;
};

class ReadReplyPacketBuilder final : public PacketBuilderBase<ReadReplyPacketConfig> {
 public:
  using PacketBuilderBase<ReadReplyPacketConfig>::PacketBuilderBase;
  auto build() -> void override;
};

};  // namespace SpwRmap
