// Copyright (c) 2025 Gen
// Licensed under the MIT License. See LICENSE file for details.
#pragma once

#include <chrono>
#include <cstring>
#include <expected>
#include <memory>
#include <mutex>

#include "spw_rmap/internal/spw_rmap_tcp_node_impl.hh"
#include "spw_rmap/internal/tcp_client.hh"
#include "spw_rmap/internal/tcp_server.hh"

namespace spw_rmap {

using namespace std::chrono_literals;

class SpwRmapTCPClient
    : public internal::SpwRmapTCPNodeImpl<internal::TCPClient> {
 public:
  using SpwRmapTCPNodeImpl::SpwRmapTCPNodeImpl;

  std::mutex shutdown_mtx_;
  bool shutdowned_ = false;

  auto connect(std::chrono::microseconds connect_timeout = 100ms)
      -> std::expected<std::monostate, std::error_code> {
    std::lock_guard<std::mutex> lock(shutdown_mtx_);
    getBackend_() =
        std::make_unique<internal::TCPClient>(getIpAddress_(), getPort_());
    auto res = getBackend_()->connect(connect_timeout);
    shutdowned_ = false;
    if (!res.has_value()) {
      getBackend_()->disconnect();
      return std::unexpected{res.error()};
    }
    auto timeout_res = getBackend_()->setSendTimeout(getSendTimeout_());
    if (!timeout_res.has_value()) {
      getBackend_()->disconnect();
      return std::unexpected{timeout_res.error()};
    }
    return {};
  }

  auto setSendTimeout(std::chrono::microseconds timeout) noexcept
      -> std::expected<std::monostate, std::error_code> {
    std::lock_guard<std::mutex> lock(shutdown_mtx_);
    return setSendTimeoutInternal_(timeout);
  }

  auto shutdown() noexcept
      -> std::expected<std::monostate, std::error_code> override {
    std::lock_guard<std::mutex> lock(shutdown_mtx_);
    if (getBackend_()) {
      auto res = getBackend_()->shutdown();
      shutdowned_ = true;
      if (!res.has_value()) {
        return std::unexpected{res.error()};
      }
      getBackend_() = nullptr;
    }
    return {};
  }

  auto isShutdowned() noexcept -> bool override {
    std::lock_guard<std::mutex> lock(shutdown_mtx_);
    return shutdowned_;
  }
};

class SpwRmapTCPServer
    : public internal::SpwRmapTCPNodeImpl<internal::TCPServer> {
 public:
  explicit SpwRmapTCPServer(SpwRmapTCPNodeConfig config) noexcept
      : internal::SpwRmapTCPNodeImpl<internal::TCPServer>(std::move(config)) {
    getBackend_() =
        std::make_unique<internal::TCPServer>(config.ip_address, config.port);
  }

  std::mutex shutdown_mtx_;
  bool shutdowned_ = false;

  auto acceptOnce() -> std::expected<std::monostate, std::error_code> {
    std::lock_guard<std::mutex> lock(shutdown_mtx_);
    auto res = getBackend_()->accept_once();
    if (!res.has_value()) {
      std::cerr << "Failed to accept TCP connection: " << res.error().message()
                << "\n";
      return std::unexpected{res.error()};
    }
    auto timeout_res = getBackend_()->setSendTimeout(getSendTimeout_());
    if (!timeout_res.has_value()) {
      std::cerr << "Failed to set send timeout: "
                << timeout_res.error().message() << "\n";
      (void)getBackend_()->shutdown();
      return std::unexpected{timeout_res.error()};
    }
    shutdowned_ = false;
    return {};
  }

  auto setSendTimeout(std::chrono::microseconds timeout) noexcept
      -> std::expected<std::monostate, std::error_code> {
    std::lock_guard<std::mutex> lock(shutdown_mtx_);
    return setSendTimeoutInternal_(timeout);
  }

  auto shutdown() noexcept
      -> std::expected<std::monostate, std::error_code> override {
    std::lock_guard<std::mutex> lock(shutdown_mtx_);
    if (getBackend_()) {
      auto res = getBackend_()->shutdown();
      shutdowned_ = true;
      if (!res.has_value()) {
        return std::unexpected{res.error()};
      }
    }
    return {};
  }

  auto isShutdowned() noexcept -> bool override {
    std::lock_guard<std::mutex> lock(shutdown_mtx_);
    return shutdowned_;
  }
};

};  // namespace spw_rmap
