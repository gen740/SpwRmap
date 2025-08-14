#pragma once

#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

#include "SpwRmap/SpwRmapBase.hh"
#include "SpwRmap/internal/TCPClient.hh"

namespace SpwRmap {

class SpwRmap : public SpwRmapBase {
 private:
  std::unique_ptr<internal::TCPClient> tcp_client_;

 public:
  explicit SpwRmap(std::string_view ip_address, uint32_t port) {
    try {
      tcp_client_ = std::make_unique<internal::TCPClient>(ip_address, port);
    } catch (const std::system_error& e) {
      throw std::runtime_error(std::string("Failed to connect to ") + std::string(ip_address) +
                               ":" + std::to_string(port) + ": " + e.what());
    }
  }
};

};  // namespace SpwRmap
