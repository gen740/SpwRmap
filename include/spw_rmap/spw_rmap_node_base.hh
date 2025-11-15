// Copyright (c) 2025 Gen
// Licensed under the MIT License. See LICENSE file for details.
#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <future>
#include <memory>
#include <span>
#include <system_error>
#include <variant>

#include "spw_rmap/packet_parser.hh"
#include "spw_rmap/target_node.hh"

namespace spw_rmap {

class SpwRmapNodeBase {
 public:
  SpwRmapNodeBase() = default;
  virtual ~SpwRmapNodeBase() = default;

  SpwRmapNodeBase(const SpwRmapNodeBase&) = delete;
  auto operator=(const SpwRmapNodeBase&) -> SpwRmapNodeBase& = delete;

  SpwRmapNodeBase(SpwRmapNodeBase&&) = delete;
  auto operator=(SpwRmapNodeBase&&) -> SpwRmapNodeBase& = delete;

  /**
   * @brief Runs the main loop of the node.
   *
   */

  virtual auto poll() -> void = 0;

  virtual auto runLoop() -> void = 0;

  virtual auto registerOnWrite(std::function<void(Packet)> onWrite) noexcept
      -> void = 0;

  virtual auto registerOnRead(std::function<void(Packet)> onRead) noexcept
      -> void = 0;

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
  virtual auto write(std::shared_ptr<TargetNodeBase> target_node,
                     uint32_t memory_address,
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
  virtual auto read(std::shared_ptr<TargetNodeBase> target_node,
                    uint32_t memory_address,
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
  virtual auto writeAsync(std::shared_ptr<TargetNodeBase> target_node,
                          uint32_t memory_address,
                          const std::span<const uint8_t> data,
                          std::function<void(Packet)> on_complete) noexcept
      -> std::future<std::expected<std::monostate, std::error_code>> = 0;

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
  virtual auto readAsync(std::shared_ptr<TargetNodeBase> target_node,
                         uint32_t memory_address, uint32_t data_length,
                         std::function<void(Packet)> on_complete) noexcept
      -> std::future<std::expected<std::monostate, std::error_code>> = 0;

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

}  // namespace spw_rmap
