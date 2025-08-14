#pragma once

#include <chrono>
#include <cstdint>
#include <span>

namespace SpwRmap::internal {

using namespace std::chrono_literals;
class TCPClient {
 private:
  int fd_ = -1;

  static auto close_retry_(int& fd) -> void;

 public:
  TCPClient() = delete;
  TCPClient(const TCPClient&) = delete;
  auto operator=(const TCPClient&) -> TCPClient& = delete;
  TCPClient(TCPClient&&) = delete;
  auto operator=(TCPClient&&) -> TCPClient& = delete;

  TCPClient(std::string_view ip_address, uint32_t port,
            std::chrono::microseconds recv_timeout = 200ms,
            std::chrono::microseconds send_timeout = 200ms,
            std::chrono::microseconds connect_timeout = 500ms);

  ~TCPClient();

  auto setRecvTimeout(std::chrono::microseconds timeout) -> void;

  auto setSendTimeout(std::chrono::microseconds timeout) -> void;

  auto send_all(std::span<const uint8_t> data) -> void;

  auto recv_some(std::span<uint8_t> buf) -> size_t;

 private:
};

}  // namespace SpwRmap::internal
