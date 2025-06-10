/**
 * @file DummySpwRmap.hh
 * @brief DummySpwRmap class definition
 * @date 2025-03-01
 * @author gen740
 */
#pragma once

#include <span>
#include <string_view>
#include <vector>

#include "SpwRmap/SpwRmapBase.hh"

namespace SpwRmap {

class DummySpwRmap final : public SpwRmapBase {
 public:
  explicit DummySpwRmap(std::string_view ip_address, uint32_t port)
      : ip_address(ip_address), port(port) {}
  ~DummySpwRmap() override = default;

  auto addTargetNode([[maybe_unused]] const TargetNode &target_node)
      -> void final {
    // Do nothing
  }

  auto write([[maybe_unused]] uint8_t logical_address,
             [[maybe_unused]] uint32_t memory_address,
             [[maybe_unused]] const std::span<uint8_t> data) -> void final {
    // Do nothing
  }

  [[nodiscard]] auto read([[maybe_unused]] uint8_t logical_address,
                          [[maybe_unused]] uint32_t memory_address,
                          [[maybe_unused]] uint32_t length)
      -> std::vector<uint8_t> final {
    auto ret = std::vector<uint8_t>(length, 0);
    return ret;
  }

  auto emitTimeCode([[maybe_unused]] uint8_t timecode) -> void final {
    // Do nothing
  }

 private:
  [[maybe_unused]] std::string ip_address;
  [[maybe_unused]] uint32_t port;
};

}  // namespace SpwRmap
