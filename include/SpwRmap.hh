#pragma once

#include <memory>
#include <span>
#include <vector>

namespace SpwRmap {

struct TargetNode {
  uint8_t logical_address;
  std::vector<uint8_t> target_spacewire_address;
  std::vector<uint8_t> reply_address;
};

class SpwRmap {

private:
  class SpwPImpl;
  std::shared_ptr<SpwPImpl> pImpl;

public:
  SpwRmap(std::string_view ip_address, uint32_t port);

  auto addTargetNode(const TargetNode &target_node) -> void;
  auto write(uint8_t logical_address, uint32_t memory_address,
             const std::span<uint8_t> data) -> void;
  auto read(uint8_t logical_address, uint32_t memory_address, uint32_t length)
      -> std::vector<uint8_t>;
};

} // namespace SpwRmap
