#pragma once

#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "SpwRmap/SpwRmapBase.hh"
#include "SpwRmap/internal/TCPClient.hh"

namespace SpwRmap {

class SpwRmap : public SpwRmapBase {
 private:
  std::unique_ptr<internal::TCPClient> tcp_client_;
  std::thread worker_thread_;

  std::vector<uint8_t> buffer_;
  std::span<uint8_t> buffer1_;
  std::span<uint8_t> buffer2_;
  std::string port_;

 public:
  explicit SpwRmap(std::string_view ip_address, uint32_t port,
                   size_t buffer_size = 1024) {
    try {
      port_ = std::to_string(port);
      tcp_client_ = std::make_unique<internal::TCPClient>(ip_address, port_);
      std::ignore = tcp_client_->connect(
          std::chrono::milliseconds(5000), std::chrono::milliseconds(5000),
          std::chrono::milliseconds(5000));  // TODO: Handle expected

    } catch (const std::system_error& e) {
      throw std::runtime_error(std::string("Failed to connect to ") +
                               std::string(ip_address) + ":" +
                               std::to_string(port) + ": " + e.what());
    }
    buffer_.resize(buffer_size * 2);
    buffer1_ = std::span<uint8_t>(buffer_.data(), buffer_size);
    buffer2_ = std::span<uint8_t>(buffer_.data() + buffer_size, buffer_size);
    // worker_thread_ = std::thread([]() {
    //   while (true) {
    //     // // Implement the worker thread logic here, if needed.
    //     // std::this_thread::sleep_for(std::chrono::milliseconds(100));
    //   }
    // });
  }
};

};  // namespace SpwRmap
