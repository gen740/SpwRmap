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
  std::unique_ptr<std::vector<uint8_t>> packet_vec_ = nullptr;
  std::span<uint8_t> packet_;

  size_t total_size_ = 0;

 protected:
  [[nodiscard]] auto getConfig_() const noexcept -> const ConfigT& {
    return config_;
  }

  [[nodiscard]] auto getPacket_() noexcept -> std::span<uint8_t> {
    return packet_;
  }

  auto setPacket_(std::span<uint8_t> buffer) noexcept -> void {
    packet_ = buffer;
  }

  [[nodiscard]] auto usingInternalBuffer_() const noexcept -> bool {
    return packet_vec_ != nullptr;
  }

  auto resizeInternalBuffer_(size_t size) const noexcept -> void {
    return packet_vec_->resize(size);
  }

  auto setTotalSize_(size_t size) noexcept -> void { total_size_ = size; }

  [[nodiscard]] virtual auto calcTotalSize_() const noexcept -> size_t = 0;

 public:
  virtual ~PacketBuilderBase() = default;
  explicit PacketBuilderBase() noexcept = default;
  explicit PacketBuilderBase(ConfigT config) noexcept
      : config_(std::move(config)) {}
  auto setConfig(ConfigT config) noexcept { config_ = std::move(config); }
  auto getMutableConfig() noexcept -> ConfigT& { return config_; }
  [[nodiscard]] auto getConfig() const noexcept -> const ConfigT& {
    return config_;
  }
  auto reservePacket(size_t size) -> void {
    packet_vec_ = std::make_unique<std::vector<uint8_t>>(size);
    packet_ = std::span<uint8_t>(*packet_vec_);
  }
  auto setBuffer(std::span<uint8_t> buffer) -> void {
    packet_ = buffer;
    if (packet_vec_) {
      packet_vec_.reset();
    }
  }

  [[nodiscard]] auto getTotalSize() const noexcept -> size_t {
    return total_size_;
  }

  [[nodiscard]] auto getPacket() const noexcept -> std::span<const uint8_t> {
    return packet_.subspan(0, total_size_);
  }

  PacketBuilderBase(const PacketBuilderBase&) = delete;
  auto operator=(const PacketBuilderBase&) -> PacketBuilderBase& = delete;
  PacketBuilderBase(PacketBuilderBase&&) = delete;
  auto operator=(PacketBuilderBase&&) -> PacketBuilderBase& = delete;

  auto build() -> void {
    buildImpl();
  }

  virtual auto buildImpl() -> void = 0;
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
 private:
  [[nodiscard]] auto calcTotalSize_() const noexcept -> size_t override;

 public:
  using PacketBuilderBase<ReadPacketConfig>::PacketBuilderBase;
  auto buildImpl() -> void override;
};

class WritePacketBuilder final : public PacketBuilderBase<WritePacketConfig> {
 public:
  using PacketBuilderBase<WritePacketConfig>::PacketBuilderBase;
  auto buildImpl() -> void override;
  [[nodiscard]] auto calcTotalSize_() const noexcept -> size_t override;
};

class WriteReplyPacketBuilder final
    : public PacketBuilderBase<WriteReplyPacketConfig> {
 public:
  using PacketBuilderBase<WriteReplyPacketConfig>::PacketBuilderBase;
  auto buildImpl() -> void override;
  [[nodiscard]] auto calcTotalSize_() const noexcept -> size_t override;
};

class ReadReplyPacketBuilder final
    : public PacketBuilderBase<ReadReplyPacketConfig> {
 public:
  using PacketBuilderBase<ReadReplyPacketConfig>::PacketBuilderBase;
  auto buildImpl() -> void override;
  [[nodiscard]] auto calcTotalSize_() const noexcept -> size_t override;
};

};  // namespace SpwRmap
