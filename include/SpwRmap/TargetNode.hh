/**
 * @file TargetNode.hh
 * @brief Defines the TargetNode struct.
 * @date 2025-03-01
 * @author gen740
 */
#pragma once

#include <cstdint>
#include <vector>

namespace SpwRmap {

struct TargetNode {
  /**
   * @brief Logical address for SpaceWire RMAP.
   *
   * Represents an 8-bit logical address used in SpaceWire RMAP.
   * Valid addresses are integers greater than or equal to 32.
   */
  uint8_t logical_address{};

  std::vector<uint8_t> target_spacewire_address{};
  std::vector<uint8_t> reply_address{};
};

};  // namespace SpwRmap
