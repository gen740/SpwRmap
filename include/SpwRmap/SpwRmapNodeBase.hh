/**
 * @file SpwRmapBase.hh
 * @brief Base class for SpaceWire RMAP communication
 * @date 2025-03-01
 * @author gen740
 */

#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <future>
#include <span>
#include <system_error>
#include <variant>

#include "SpwRmap/TargetNode.hh"

namespace SpwRmap {

class SpwRmapNodeBase {
 public:
  SpwRmapNodeBase() = default;
  virtual ~SpwRmapNodeBase() = default;

  SpwRmapNodeBase(const SpwRmapNodeBase&) = delete;
  auto operator=(const SpwRmapNodeBase&) -> SpwRmapNodeBase& = delete;

  SpwRmapNodeBase(SpwRmapNodeBase&&) = delete;
  auto operator=(SpwRmapNodeBase&&) -> SpwRmapNodeBase& = delete;

  // /**
  //  *
  //  */
  // virtual auto runLoop() -> void = 0;

  /**
   * @brief Writes data to a target node.
   *
   * This function sends data to a specific memory address of the target node.
   * The write operation is performed synchronously.
   *
   * @param logical_address Logical address of the target node.
   * @param memory_address Target memory address.
   * @param data Data to write.
   */
  virtual auto write(const TargetNodeBase& target_node, uint32_t memory_address,
                     const std::span<const uint8_t> data) noexcept
      -> std::expected<std::monostate, std::error_code> = 0;

  /**
   * @brief Reads data from a target node.
   *
   * This function retrieves data from a specific memory address of the target
   * node. The read operation is performed synchronously.
   *
   * @param logical_address Logical address of the target node.
   * @param memory_address Target memory address.
   * @param data Reference to a span where the read data will be stored.
   */
  virtual auto read(const TargetNodeBase& target_node, uint32_t memory_address,
                    const std::span<uint8_t> data) noexcept
      -> std::expected<std::monostate, std::error_code> = 0;

  /**
   * @brief Writes data to a target node.
   *
   * This function sends data to a specific memory address of the target node.
   * The write operation is performed synchronously.
   *
   * @param logical_address Logical address of the target node.
   * @param memory_address Target memory address.
   * @param data Data to write.
   */
  virtual auto writeAsync(const TargetNodeBase& target_node,
                          uint32_t memory_address,
                          const std::span<const uint8_t> data,
                          std::function<void()> onComplete) noexcept
      -> std::future<std::expected<std::monostate, std::error_code>> {
    std::ignore = target_node;
    std::ignore = memory_address;
    std::ignore = data;
    std::ignore = onComplete;
    std::promise<std::expected<std::monostate, std::error_code>> promise;
    promise.set_value(
        std::unexpected(std::make_error_code(std::errc::not_supported)));
    return promise.get_future();
  }

  /**
   * @brief Reads data from a target node.
   *
   * This function retrieves data from a specific memory address of the target
   * node. The read operation is performed synchronously.
   *
   * @param logical_address Logical address of the target node.
   * @param memory_address Target memory address.
   * @param data Reference to a span where the read data will be stored.
   */
  virtual auto readAsync(
      const TargetNodeBase& target_node, uint32_t memory_address,
      std::function<void(std::span<const uint8_t>)> onComplete) noexcept
      -> std::future<std::expected<std::monostate, std::error_code>> {
    std::ignore = target_node;
    std::ignore = memory_address;
    std::ignore = onComplete;
    std::promise<std::expected<std::monostate, std::error_code>> promise;
    promise.set_value(
        std::unexpected(std::make_error_code(std::errc::not_supported)));
    return promise.get_future();
  }

  /**
   * @brief Emits a time code.
   *
   * Sends a 6-bit time code. The upper 2 bits are ignored.
   *
   * @param timecode Time code to emit.
   */
  virtual auto emitTimeCode(uint8_t timecode) noexcept
      -> std::expected<std::monostate, std::error_code> = 0;
};

}  // namespace SpwRmap
