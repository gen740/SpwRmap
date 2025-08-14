#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <span>
#include <system_error>

namespace SpwRmap::internal {

using namespace std::chrono_literals;
class TCPClient {
 private:
  int fd_ = -1;

  static auto close_retry_(int fd) noexcept -> void;
  std::string_view ip_address_;
  std::string_view port_;

 public:
  TCPClient() = delete;
  TCPClient(const TCPClient&) = delete;
  auto operator=(const TCPClient&) -> TCPClient& = delete;
  TCPClient(TCPClient&&) = delete;
  auto operator=(TCPClient&&) -> TCPClient& = delete;

  TCPClient(std::string_view ip_address, std::string_view port)
      : ip_address_(ip_address), port_(port) {}

  ~TCPClient();

  [[nodiscard]] auto connect(std::chrono::microseconds recv_timeout,
                             std::chrono::microseconds send_timeout,
                             std::chrono::microseconds connect_timeout) noexcept
      -> std::expected<std::monostate, std::error_code>;

  auto disconnect() noexcept -> void;

  [[nodiscard]] auto reconnect(
      std::chrono::microseconds recv_timeout,
      std::chrono::microseconds send_timeout,
      std::chrono::microseconds connect_timeout) noexcept
      -> std::expected<std::monostate, std::error_code>;

  [[nodiscard]] auto setRecvTimeout(std::chrono::microseconds timeout) noexcept
      -> std::expected<std::monostate, std::error_code>;

  [[nodiscard]] auto setSendTimeout(std::chrono::microseconds timeout) noexcept
      -> std::expected<std::monostate, std::error_code>;

  [[nodiscard]] auto sendAll(std::span<const uint8_t> data) noexcept
      -> std::expected<std::monostate, std::error_code>;

  [[nodiscard]] auto recvSome(std::span<uint8_t> buf) noexcept
      -> std::expected<size_t, std::error_code>;

 private:
};

}  // namespace SpwRmap::internal
