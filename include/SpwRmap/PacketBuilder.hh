#pragma once

#include <cassert>
#include <cstdint>
#include <expected>
#include <print>
#include <span>
#include <system_error>
#include <vector>

namespace SpwRmap {

/**
 * @class PacketBuilderBase
 * @brief Base class for packet builders.
 *
 * @tparam ConfigT The type of the configuration used to build the packet.
 */
template <class ConfigT>
class PacketBuilderBase {
 private:
  std::unique_ptr<std::vector<uint8_t>> packet_vec_ = nullptr;
  std::span<uint8_t> packet_;

  size_t total_size_ = 0;

  /**
   * @brief Resize the internal buffer to the specified size.
   *
   * @param size The new size of the internal buffer.
   */
  auto resizeInternalBuffer_(size_t size) -> void {
    assert(packet_vec_ != nullptr &&
           "Internal buffer is not initialized. Call reservePacket first.");
    packet_vec_->resize(size);
    packet_ = std::span<uint8_t>(*packet_vec_);
  }

 protected:
  /**
   * @brief Get the internal packet buffer.
   *
   * @return std::span<uint8_t> The internal packet buffer.
   */
  [[nodiscard]] auto getPacket_() noexcept -> std::span<uint8_t> {
    return packet_;
  }

  /**
   * @brief Get the internal packet buffer (const version).
   *
   * @return const std::span<uint8_t> The internal packet buffer.
   */
  [[nodiscard]] auto getPacket_() const noexcept -> std::span<const uint8_t> {
    return packet_;
  }

  /**
   * @brief Check if the internal buffer is being used.
   *
   * @return bool
   */
  [[nodiscard]] auto usingInternalBuffer_() const noexcept -> bool {
    return packet_vec_ != nullptr;
  }

  /**
   * @brief Calculate the total size of the packet based on the configuration.
   *
   * @param config The configuration object containing parameters for the
   * packet.
   * @return size_t The total size of the packet.
   */
  [[nodiscard]] virtual auto calcTotalSize_(
      const ConfigT& config) const noexcept -> size_t = 0;

 public:
  virtual ~PacketBuilderBase() = default;
  explicit PacketBuilderBase() noexcept = default;
  PacketBuilderBase(const PacketBuilderBase&) = delete;
  auto operator=(const PacketBuilderBase&) -> PacketBuilderBase& = delete;
  PacketBuilderBase(PacketBuilderBase&&) = delete;
  auto operator=(PacketBuilderBase&&) -> PacketBuilderBase& = delete;

  /**
   * @brief Reserve a packet buffer of the specified size.
   *
   * This function allocates a new vector to hold the packet data.
   *
   * @param size The size of the packet buffer to reserve.
   */
  auto reservePacket(size_t size) -> void {
    if (usingInternalBuffer_()) {
      resizeInternalBuffer_(size);
      return;
    }
    packet_vec_ = std::make_unique<std::vector<uint8_t>>(size);
    packet_ = std::span<uint8_t>(*packet_vec_);
    total_size_ = 0;
  }

  /**
   * @brief Set the internal packet buffer to a pre-allocated span.
   *
   * This function allows the user to set the packet buffer directly,
   * bypassing the internal vector allocation.
   *
   * This function should be used with caution, as it does not check the
   * lifetime or ownership of the provided buffer.
   *
   * @param buffer The pre-allocated span to use as the packet buffer.
   */
  auto setBuffer(std::span<uint8_t> buffer) noexcept -> void {
    packet_ = buffer;
    if (packet_vec_) {
      packet_vec_.reset();
    }
    total_size_ = 0;
  }

  /**
   * @brief Get the internal packet buffer as a span.
   *
   * This function returns the internal packet buffer as a span.
   *
   * @return std::span<const uint8_t> The internal packet buffer.
   */
  [[nodiscard]] auto getPacket() const noexcept
      -> std::expected<std::span<const uint8_t>, std::error_code> {
    if (packet_.size() < total_size_) {
      return std::unexpected{
          std::make_error_code(std::errc::result_out_of_range)};
    }
    return packet_.subspan(0, total_size_);
  }

  /**
   * @brief Get the total size of the packet.
   * @return size_t The total size of the packet.
   */
  [[nodiscard]] auto getTotalSize() const noexcept -> size_t {
    return total_size_;
  }

  /**
   * @brief Build the packet based on the provided configuration.
   *
   * @param config The configuration object containing parameters for the
   * packet.
   * @return std::expected<std::monostate, std::error_code> An expected
   * object indicating success or failure.
   */
  [[nodiscard]]
  auto build(const ConfigT& config)
      -> std::expected<std::monostate, std::error_code> {
    size_t total_size = calcTotalSize_(config);
    if (getPacket_().size() < total_size) {
      if (usingInternalBuffer_()) {
        resizeInternalBuffer_(total_size);
      } else if (getPacket_().size() == 0) {
        reservePacket(total_size);
      } else {
        return std::unexpected{
            std::make_error_code(std::errc::result_out_of_range)};
      }
    }
    total_size_ = total_size;
    buildImpl(config);
    return {};
  }

  /**
   * @brief Build the packet implementation.
   *
   * This function is called by `build` after the total size and buffer
   * have been set up.
   * It can be assumed that the `packet_` has been resized to the total size
   *
   * @param config The configuration object containing parameters for the
   * packet.
   */
  virtual auto buildImpl(const ConfigT& config) noexcept -> void = 0;
};

struct ReadPacketConfig {
  std::span<const uint8_t> targetSpaceWireAddress;
  std::span<const uint8_t> replyAddress;
  uint8_t targetLogicalAddress{0};
  uint8_t initiatorLogicalAddress{0xFE};
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
  uint8_t initiatorLogicalAddress{0xFE};
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
  uint8_t initiatorLogicalAddress{0xFE};
  uint8_t status{0};
  uint8_t targetLogicalAddress{0};
  uint16_t transactionID{0};
  std::span<const uint8_t> data;
  bool incrementMode{true};
};

struct WriteReplyPacketConfig {
  std::span<const uint8_t> replyAddress;
  uint8_t initiatorLogicalAddress{0xFE};
  uint8_t status{0};
  uint8_t targetLogicalAddress{0};
  uint16_t transactionID{0};
  bool incrementMode{true};
  bool verifyMode{true};
};

/**
 * @class ReadPacketBuilder
 *
 * @brief A class for building RMAP read packets.
 */
class ReadPacketBuilder final : public PacketBuilderBase<ReadPacketConfig> {
 private:
  [[nodiscard]] auto calcTotalSize_(
      const ReadPacketConfig& config) const noexcept -> size_t override;

 public:
  using PacketBuilderBase<ReadPacketConfig>::PacketBuilderBase;
  auto buildImpl(const ReadPacketConfig& config) noexcept -> void override;
};

/**
 * @class WritePacketBuilder
 *
 * @brief A class for building RMAP write packets.
 */
class WritePacketBuilder final : public PacketBuilderBase<WritePacketConfig> {
 private:
  [[nodiscard]] auto calcTotalSize_(
      const WritePacketConfig& config) const noexcept -> size_t override;

 public:
  using PacketBuilderBase<WritePacketConfig>::PacketBuilderBase;
  auto buildImpl(const WritePacketConfig& config) noexcept -> void override;
};

/**
 * @class ReadReplyPacketBuilder
 *
 * @brief A class for building RMAP read reply packets.
 */
class WriteReplyPacketBuilder final
    : public PacketBuilderBase<WriteReplyPacketConfig> {
 private:
  [[nodiscard]] auto calcTotalSize_(
      const WriteReplyPacketConfig& config) const noexcept -> size_t override;

 public:
  using PacketBuilderBase<WriteReplyPacketConfig>::PacketBuilderBase;
  auto buildImpl(const WriteReplyPacketConfig& config) noexcept
      -> void override;
};

/**
 * @class ReadReplyPacketBuilder
 *
 * @brief A class for building RMAP read reply packets.
 */
class ReadReplyPacketBuilder final
    : public PacketBuilderBase<ReadReplyPacketConfig> {
 private:
  [[nodiscard]] auto calcTotalSize_(
      const ReadReplyPacketConfig& config) const noexcept -> size_t override;

 public:
  using PacketBuilderBase<ReadReplyPacketConfig>::PacketBuilderBase;
  auto buildImpl(const ReadReplyPacketConfig& config) noexcept -> void override;
};

};  // namespace SpwRmap
