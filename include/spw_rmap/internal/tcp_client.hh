// Copyright (c) 2025 Gen
// Licensed under the MIT License. See LICENSE file for details.
#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <mutex>
#include <span>
#include <system_error>

namespace spw_rmap::internal {

using namespace std::chrono_literals;

/**
 * @class TCPClient
 * @brief A class for managing TCP connections.
 *
 * This TCPClient are supposed to be used for RMAP communication over TCP.
 */
class TCPClient {
 private:
  int fd_ = -1;

  static auto close_retry_(int fd) noexcept -> void;
  std::string_view ip_address_;
  std::string_view port_;
  std::mutex mtx_;

 public:
  TCPClient() = delete;
  TCPClient(const TCPClient&) = delete;
  auto operator=(const TCPClient&) -> TCPClient& = delete;
  TCPClient(TCPClient&&) = delete;
  auto operator=(TCPClient&&) -> TCPClient& = delete;

  TCPClient(std::string_view ip_address, std::string_view port)
      : ip_address_(ip_address), port_(port) {}

  ~TCPClient();

  [[nodiscard]] auto connect(
      std::chrono::microseconds recv_timeout = 500ms,
      std::chrono::microseconds send_timeout = 500ms,
      std::chrono::microseconds connect_timeout = 500ms) noexcept
      -> std::expected<std::monostate, std::error_code>;

  auto disconnect() noexcept -> void;

  [[nodiscard]] auto reconnect(
      std::chrono::microseconds recv_timeout = 500ms,
      std::chrono::microseconds send_timeout = 500ms,
      std::chrono::microseconds connect_timeout = 500ms) noexcept
      -> std::expected<std::monostate, std::error_code>;

  [[nodiscard]] auto setRecvTimeout(std::chrono::microseconds timeout) noexcept
      -> std::expected<std::monostate, std::error_code>;

  [[nodiscard]] auto setSendTimeout(std::chrono::microseconds timeout) noexcept
      -> std::expected<std::monostate, std::error_code>;

  [[nodiscard]] auto sendAll(std::span<const uint8_t> data) noexcept
      -> std::expected<std::monostate, std::error_code>;

  [[nodiscard]] auto recvSome(std::span<uint8_t> buf) noexcept
      -> std::expected<size_t, std::error_code>;

  [[nodiscard]] auto shutdown() noexcept
      -> std::expected<std::monostate, std::error_code>;
};

}  // namespace spw_rmap::internal
