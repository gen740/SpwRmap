// Copyright (c) 2025 Gen
// Licensed under the MIT License. See LICENSE file for details.
#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <span>

namespace spw_rmap::internal {

using namespace std::chrono_literals;

class TCPServer {
 private:
  int listen_fd_ = -1;  // listening socket
  int client_fd_ = -1;  // accepted client socket

  static auto close_retry_(int fd) noexcept -> void;
  std::string_view bind_address_;
  std::string_view port_;

 public:
  TCPServer() = delete;
  TCPServer(const TCPServer&) = delete;
  auto operator=(const TCPServer&) -> TCPServer& = delete;
  TCPServer(TCPServer&&) = delete;
  auto operator=(TCPServer&&) -> TCPServer& = delete;

  TCPServer(std::string_view bind_address, std::string_view port) noexcept
      : bind_address_(bind_address), port_(port) {};

  ~TCPServer() noexcept;

  auto accept_once(std::chrono::microseconds send_timeout = 200ms,
                   std::chrono::microseconds recv_timeout = 200ms) noexcept
      -> std::expected<std::monostate, std::error_code>;

  auto setRecvTimeout(std::chrono::microseconds timeout) noexcept
      -> std::expected<std::monostate, std::error_code>;

  auto setSendTimeout(std::chrono::microseconds timeout) noexcept
      -> std::expected<std::monostate, std::error_code>;

  auto sendAll(std::span<const uint8_t> data) noexcept
      -> std::expected<std::monostate, std::error_code>;

  auto recvSome(std::span<uint8_t> buf) noexcept
      -> std::expected<size_t, std::error_code>;

  auto shutdown() noexcept -> std::expected<std::monostate, std::error_code>;
};

}  // namespace spw_rmap::internal
