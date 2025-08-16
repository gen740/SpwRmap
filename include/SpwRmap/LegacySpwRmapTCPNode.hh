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
#include <string_view>

#include "SpwRmap/SpwRmapNodeBase.hh"
#include "SpwRmap/TargetNode.hh"

namespace SpwRmap {

class LegacySpwRmapTCPNode final : public SpwRmapNodeBase {
 public:
  LegacySpwRmapTCPNode(const LegacySpwRmapTCPNode &) = delete;
  LegacySpwRmapTCPNode(LegacySpwRmapTCPNode &&) = delete;
  auto operator=(const LegacySpwRmapTCPNode &) -> LegacySpwRmapTCPNode & = delete;
  auto operator=(LegacySpwRmapTCPNode &&) -> LegacySpwRmapTCPNode & = delete;
  explicit LegacySpwRmapTCPNode(std::string_view ip_address, uint32_t port);
  ~LegacySpwRmapTCPNode() override;

  auto write(const TargetNodeBase &target_node, uint32_t memory_address,
             const std::span<const uint8_t> data) noexcept
      -> std::expected<std::monostate, std::error_code> final;

  auto read(const TargetNodeBase &target_node, uint32_t memory_address,
            const std::span<uint8_t> data) noexcept
      -> std::expected<std::monostate, std::error_code> final;

  auto emitTimeCode(uint8_t timecode) noexcept
      -> std::expected<std::monostate, std::error_code> final;

 private:
  class SpwPImpl;
  std::unique_ptr<SpwPImpl> impl_;
};

}  // namespace SpwRmap
