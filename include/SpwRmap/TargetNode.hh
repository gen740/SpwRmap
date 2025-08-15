/**
 * @file TargetNode.hh
 * @brief Defines the TargetNode struct.
 * @date 2025-03-01
 * @author gen740
 */
#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace SpwRmap {

class TargetNodeBase {
 private:
  uint8_t logical_address_{};
  uint8_t initiator_logical_address_{};

 public:
  TargetNodeBase(uint8_t logical_address = 0x00,
                 uint8_t initiator_logical_address = 0xFE) noexcept
      : logical_address_(logical_address),
        initiator_logical_address_(initiator_logical_address) {}
  TargetNodeBase(const TargetNodeBase &) = default;
  TargetNodeBase(TargetNodeBase &&) = default;
  auto operator=(const TargetNodeBase &) -> TargetNodeBase & = default;
  auto operator=(TargetNodeBase &&) -> TargetNodeBase & = default;
  virtual ~TargetNodeBase() = default;

  [[nodiscard]] auto getTargetLogicalAddress() const noexcept -> uint8_t {
    return logical_address_;
  }

  [[nodiscard]] auto getInitiatorLogicalAddress() const noexcept -> uint8_t {
    return initiator_logical_address_;
  }

  [[nodiscard]] virtual auto getTargetSpaceWireAddress() const noexcept
      -> std::span<const uint8_t> = 0;

  [[nodiscard]] virtual auto getReplyAddress() const noexcept
      -> std::span<const uint8_t> = 0;
};

template <size_t TargetLength, size_t ReplyLength>
class TargetNodeFixed : public TargetNodeBase {
 private:
  std::array<uint8_t, TargetLength> target_spacewire_address{};
  std::array<uint8_t, ReplyLength> reply_address{};

 public:
  TargetNodeFixed(uint8_t logical_address,
                  std::array<uint8_t, TargetLength> &&target_spacewire_address,
                  std::array<uint8_t, ReplyLength> &&reply_address,
                  uint8_t initiator_logical_address = 0xFE) noexcept
      : TargetNodeBase(logical_address, initiator_logical_address),
        target_spacewire_address(std::move(target_spacewire_address)),
        reply_address(std::move(reply_address)) {}

  [[nodiscard]] auto getTargetSpaceWireAddress() const noexcept
      -> std::span<const uint8_t> override {
    return target_spacewire_address;
  }

  [[nodiscard]] auto getReplyAddress() const noexcept
      -> std::span<const uint8_t> override {
    return reply_address;
  };
};

class TargetNodeDynamic : public TargetNodeBase {
 private:
  std::vector<uint8_t> target_spacewire_address{};
  std::vector<uint8_t> reply_address{};

 public:
  TargetNodeDynamic(uint8_t logical_address,
                    std::vector<uint8_t> &&target_spacewire_address,
                    std::vector<uint8_t> &&reply_address,
                    uint8_t initiator_logical_address = 0xFE) noexcept
      : TargetNodeBase(logical_address, initiator_logical_address),
        target_spacewire_address(std::move(target_spacewire_address)),
        reply_address(std::move(reply_address)) {}

  [[nodiscard]] auto getTargetSpaceWireAddress() const noexcept
      -> std::span<const uint8_t> override {
    return target_spacewire_address;
  }

  [[nodiscard]] auto getReplyAddress() const noexcept
      -> std::span<const uint8_t> override {
    return reply_address;
  };
};

};  // namespace SpwRmap
