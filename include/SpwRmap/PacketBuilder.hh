#pragma once

#include <cstdint>
namespace SpwRmap {

class SpwRmapPacketBuilderBase {
 public:
  SpwRmapPacketBuilderBase() = default;
  virtual ~SpwRmapPacketBuilderBase() = default;

 protected:
  uint32_t initiatorLogicalAddress_{};
  // std::vector<uint8_t> 

};

};  // namespace SpwRmap
