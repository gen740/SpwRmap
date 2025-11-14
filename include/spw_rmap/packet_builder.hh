// Copyright (c) 2025 Gen
// Licensed under the MIT License. See LICENSE file for details.
#pragma once

#include <cassert>
#include <cstdint>
#include <expected>
#include <span>
#include <system_error>

namespace spw_rmap {

/**
 * @class PacketBuilderBase
 * @brief Base class for packet builders.
 *
 * @tparam ConfigT The type of the configuration used to build the packet.
 */
template <class ConfigT>
class PacketBuilderBase {
 private:
  size_t total_size_ = 0;

 public:
  virtual ~PacketBuilderBase() = default;
  explicit PacketBuilderBase() noexcept = default;

  /**
   * @brief Delete copy and move constructors and assignment operators.
   */
  PacketBuilderBase(const PacketBuilderBase&) = delete;
  auto operator=(const PacketBuilderBase&) -> PacketBuilderBase& = delete;
  PacketBuilderBase(PacketBuilderBase&&) = delete;
  auto operator=(PacketBuilderBase&&) -> PacketBuilderBase& = delete;

  /**
   * @brief Calculate the total size of the packet based on the configuration.
   *
   * @param config The configuration object containing parameters for the
   *        packet.
   * @return size_t The total size of the packet.
   */
  [[nodiscard]] virtual auto getTotalSize(const ConfigT& config) const noexcept
      -> size_t = 0;

  /**
   * @brief Build the packet based on the provided configuration.
   *
   * @param config The configuration object containing parameters for the
   *        packet.
   * @param out The output buffer where the packet will be built.
   *        out.size() must be at least getTotalSize(config).
   *
   * @return std::expected<size_t, std::error_code> An expected
   *         object indicating success or failure. On success, it contains the
   *         size of the built packet. On failure, it contains an error code.
   */
  [[nodiscard]]
  virtual auto build(const ConfigT& config, std::span<uint8_t> out)
      -> std::expected<size_t, std::error_code> = 0;
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
 public:
  [[nodiscard]] auto getTotalSize(const ReadPacketConfig& config) const noexcept
      -> size_t override;
  using PacketBuilderBase<ReadPacketConfig>::PacketBuilderBase;
  auto build(const ReadPacketConfig& config, std::span<uint8_t> out) noexcept
      -> std::expected<size_t, std::error_code> final;
};

/**
 * @class WritePacketBuilder
 *
 * @brief A class for building RMAP write packets.
 */
class WritePacketBuilder final : public PacketBuilderBase<WritePacketConfig> {
 public:
  [[nodiscard]] auto getTotalSize(
      const WritePacketConfig& config) const noexcept -> size_t override;

  using PacketBuilderBase<WritePacketConfig>::PacketBuilderBase;
  auto build(const WritePacketConfig& config, std::span<uint8_t> out) noexcept
      -> std::expected<size_t, std::error_code> final;
};

/**
 * @class ReadReplyPacketBuilder
 *
 * @brief A class for building RMAP read reply packets.
 */
class WriteReplyPacketBuilder final
    : public PacketBuilderBase<WriteReplyPacketConfig> {
 public:
  [[nodiscard]] auto getTotalSize(
      const WriteReplyPacketConfig& config) const noexcept -> size_t override;

  using PacketBuilderBase<WriteReplyPacketConfig>::PacketBuilderBase;
  auto build(const WriteReplyPacketConfig& config,
             std::span<uint8_t> out) noexcept
      -> std::expected<size_t, std::error_code> final;
};

/**
 * @class ReadReplyPacketBuilder
 *
 * @brief A class for building RMAP read reply packets.
 */
class ReadReplyPacketBuilder final
    : public PacketBuilderBase<ReadReplyPacketConfig> {
 public:
  [[nodiscard]] auto getTotalSize(
      const ReadReplyPacketConfig& config) const noexcept -> size_t override;

  using PacketBuilderBase<ReadReplyPacketConfig>::PacketBuilderBase;
  auto build(const ReadReplyPacketConfig& config,
             std::span<uint8_t> out) noexcept
      -> std::expected<size_t, std::error_code> final;
};

};  // namespace spw_rmap
