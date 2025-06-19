/**
 * @file SpwRmapBase.hh
 * @brief Base class for SpaceWire RMAP communication
 * @date 2025-03-01
 * @author gen740
 */

#pragma once

#include <span>

#include "SpwRmap/TargetNode.hh"

namespace SpwRmap {

class SpwRmapBase {
 protected:
  SpwRmapBase() = default;
  virtual ~SpwRmapBase() = default;

 public:
  SpwRmapBase(const SpwRmapBase &) = delete;
  auto operator=(const SpwRmapBase &) -> SpwRmapBase & = delete;

  SpwRmapBase(SpwRmapBase &&) = delete;
  auto operator=(SpwRmapBase &&) -> SpwRmapBase & = delete;

  /**
   * @brief Adds a target node to the list.
   *
   * @param target_node The target node to add.
   */
  virtual auto addTargetNode(const TargetNode &target_node) -> void = 0;

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
  virtual auto write(uint8_t logical_address, uint32_t memory_address,
                     const std::span<uint8_t> data) -> void = 0;

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
  virtual auto read(uint8_t logical_address, uint32_t memory_address, std::span<uint8_t> &&data)
      -> void = 0;

  /**
   * @brief Emits a time code.
   *
   * Sends a 6-bit time code. The upper 2 bits are ignored.
   *
   * @param timecode Time code to emit.
   */
  virtual auto emitTimeCode(uint8_t timecode) -> void = 0;
};

}  // namespace SpwRmap
