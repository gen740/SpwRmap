// Copyright (c) 2025 Gen
// Licensed under the MIT License. See LICENSE file for details.
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <variant>

namespace spw_rmap::internal {

/**
 * @class TCPServer
 * @brief A class for managing a TCP server.
 *
 * This TCPServer is supposed to be used for RMAP communication over TCP.
 * This server accepts a single connection at a time.
 */
class TCPServer {
 private:
  int listen_fd_ = -1;  // listening socket
  int client_fd_ = -1;  // accepted client socket

  static auto close_retry_(int fd) noexcept -> void;
  std::string bind_address_;
  std::string port_;

 public:
  TCPServer() = delete;
  TCPServer(const TCPServer&) = delete;
  auto operator=(const TCPServer&) -> TCPServer& = delete;
  TCPServer(TCPServer&&) = delete;
  auto operator=(TCPServer&&) -> TCPServer& = delete;

  TCPServer(std::string bind_address, std::string port) noexcept
      : bind_address_(std::move(bind_address)), port_(std::move(port)) {};

  ~TCPServer() noexcept;

  [[nodiscard]] auto accept_once() noexcept
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
