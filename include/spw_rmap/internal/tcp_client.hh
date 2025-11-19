// Copyright (c) 2025 Gen
// Licensed under the MIT License. See LICENSE file for details.
#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <span>
#include <system_error>
#include <utility>

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

  std::string ip_address_;
  std::string port_;

 public:
  TCPClient() = delete;
  TCPClient(const TCPClient&) = delete;
  auto operator=(const TCPClient&) -> TCPClient& = delete;
  TCPClient(TCPClient&&) = delete;
  auto operator=(TCPClient&&) -> TCPClient& = delete;

  TCPClient(std::string ip_address, std::string port)
      : ip_address_(std::move(ip_address)), port_(std::move(port)) {}

  ~TCPClient();

  [[nodiscard]] auto connect(std::chrono::microseconds timeout = 500ms) noexcept
      -> std::expected<std::monostate, std::error_code>;

  auto disconnect() noexcept -> void;

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
