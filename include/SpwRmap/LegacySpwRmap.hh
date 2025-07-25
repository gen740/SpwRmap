/**
 * @file LegacySpwRmap.hh
 * @brief LegacySpwRmap class definition
 *
 * This class is used pimpl to hide the dependency on the SpwPImpl class.
 *
 * @date 2025-03-01
 * @author gen740
 */
#pragma once

#include <memory>
#include <span>

#include "SpwRmap/SpwRmapBase.hh"
#include "SpwRmap/TargetNode.hh"

namespace SpwRmap {

class LegacySpwRmap final : public SpwRmapBase {
 public:
  explicit LegacySpwRmap(std::string_view ip_address, uint32_t port);
  ~LegacySpwRmap() override;

  auto addTargetNode(const TargetNode& target_node) -> void final;
  auto write(uint8_t logical_address, uint32_t memory_address, const std::span<const uint8_t> data)
      -> void final;
  auto read(uint8_t logical_address, uint32_t memory_address, const std::span<uint8_t> data)
      -> void final;
  auto emitTimeCode(uint8_t timecode) -> void final;

 private:
  class SpwPImpl;
  std::unique_ptr<SpwPImpl> impl_;
};

}  // namespace SpwRmap
