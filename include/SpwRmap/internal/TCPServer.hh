#pragma once

#include <chrono>
#include <cstdint>
#include <span>

namespace SpwRmap::internal {

using namespace std::chrono_literals;

class TCPServer {
 private:
  int listen_fd_ = -1;  // listening socket
  int client_fd_ = -1;  // accepted client socket

  static auto close_retry_(int& fd) noexcept -> void;

 public:
  TCPServer() = delete;
  TCPServer(const TCPServer&) = delete;
  auto operator=(const TCPServer&) -> TCPServer& = delete;
  TCPServer(TCPServer&&) = delete;
  auto operator=(TCPServer&&) -> TCPServer& = delete;

  // Constructor binds, listens, and accepts one client.
  TCPServer(std::string_view bind_address, uint32_t port,
            std::chrono::microseconds send_timeout = 200ms,
            std::chrono::microseconds recv_timeout = 200ms);

  ~TCPServer();

  auto setRecvTimeout(std::chrono::microseconds timeout) -> void;

  auto setSendTimeout(std::chrono::microseconds timeout) -> void;

  // Write all bytes or throw; retries on EINTR; treats 0 as transient.
  auto send_all(std::span<const uint8_t> data) -> void;

  // Read up to buf.size() bytes; returns 0 on EOF; retries on EINTR.
  auto recv_some(std::span<uint8_t> buf) -> std::size_t;
};

}  // namespace SpwRmap::internal
