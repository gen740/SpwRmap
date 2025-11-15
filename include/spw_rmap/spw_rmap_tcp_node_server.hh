// Copyright (c) 2025 Gen
// Licensed under the MIT License. See LICENSE file for details.
#pragma once

#include <cstdint>
#include <cstring>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "spw_rmap/internal/tcp_server.hh"
#include "spw_rmap/internal/thread_pool.hh"
#include "spw_rmap/packet_builder.hh"
#include "spw_rmap/packet_parser.hh"
#include "spw_rmap/spw_rmap_node_base.hh"

namespace spw_rmap {

using namespace std::chrono_literals;

enum class BufferPolicy : uint8_t {
  Fixed,       // Fixed size
  AutoResize,  // Auto resize if needed
};

struct SpwRmapTCPNodeConfig {
  std::string ip_address;  // Expect Small String optimization
  std::string port;
  size_t send_buffer_size = 4096;
  size_t recv_buffer_size = 4096;
  size_t send_pool_size = 4;
  size_t recv_pool_size = 4;
  uint16_t transaction_id_min = 0x0020;
  uint16_t transaction_id_max = 0x0040;
  BufferPolicy buffer_policy = BufferPolicy::AutoResize;
};

class SpwRmapTCPNodeServer : public SpwRmapNodeBase {
 private:
  std::unique_ptr<internal::TCPServer> tcp_server_;
  std::thread worker_thread_;

  std::string ip_address_;
  std::string port_;
  internal::ThreadPool recv_thread_pool_{4};  // thread-safe

  std::vector<uint8_t> recv_buf_ = {};
  std::vector<uint8_t> send_buf_ = {};

  std::vector<std::function<void(Packet)>> reply_callback_ = {};
  std::vector<std::unique_ptr<std::mutex>> reply_callback_mtx_;
  std::vector<bool> available_transaction_ids_ = {};
  std::mutex transaction_ids_mtx_;

  PacketParser packet_parser_ = {};
  ReadPacketBuilder read_packet_builder_ = {};
  WritePacketBuilder write_packet_builder_ = {};
  uint8_t initiator_logical_address_ = 0xFE;
  uint16_t transaction_id_min_;
  uint16_t transaction_id_max_;
  BufferPolicy buffer_policy_ = BufferPolicy::AutoResize;

  std::atomic<bool> running_{false};

  std::function<void(Packet)> on_write_callback_ = nullptr;
  std::function<std::vector<uint8_t>(Packet)> on_read_callback_ = nullptr;

 public:
  explicit SpwRmapTCPNodeServer(SpwRmapTCPNodeConfig config) noexcept
      : ip_address_(std::move(config.ip_address)),
        port_(std::move(config.port)),
        recv_buf_(config.recv_buffer_size),
        send_buf_(config.send_buffer_size),
        transaction_id_min_(config.transaction_id_min),
        transaction_id_max_(config.transaction_id_max),
        buffer_policy_(config.buffer_policy) {
    for (uint32_t i = 0; i < transaction_id_max_ - transaction_id_min_; ++i) {
      available_transaction_ids_.emplace_back(true);
      reply_callback_.emplace_back(nullptr);
      reply_callback_mtx_.emplace_back(std::make_unique<std::mutex>());
    }
    tcp_server_ = std::make_unique<internal::TCPServer>(ip_address_, port_);
  }

 public:
  auto setInitiatorLogicalAddress(uint8_t address) -> void {
    initiator_logical_address_ = address;
  }

  auto acceptOnce(std::chrono::microseconds send_timeout,
                  std::chrono::microseconds recv_timeout) {
    auto res = tcp_server_->accept_once(send_timeout, recv_timeout);
    if (!res.has_value()) {
      std::cerr << "Failed to accept TCP connection: " << res.error().message()
                << "\n";
    }
  }

 private:
  auto recvExact_(std::span<uint8_t> buffer)
      -> std::expected<std::size_t, std::error_code>;

  auto recvAndParseOnePacket_() -> std::expected<std::size_t, std::error_code>;

  auto ignoreNBytes_(std::size_t n)
      -> std::expected<std::size_t, std::error_code>;

  auto sendReadPacket_(std::shared_ptr<TargetNodeBase> target_node,
                       uint16_t transaction_id, uint32_t memory_address,
                       uint32_t data_length) noexcept
      -> std::expected<std::monostate, std::error_code>;

  auto sendWritePacket_(std::shared_ptr<TargetNodeBase> target_node,
                        uint16_t transaction_id, uint32_t memory_address,
                        const std::span<const uint8_t> data) noexcept
      -> std::expected<std::monostate, std::error_code>;

  auto getAvailableTransactionID_() noexcept
      -> std::expected<uint32_t, std::error_code> {
    std::lock_guard<std::mutex> lock(transaction_ids_mtx_);
    for (uint32_t i = 0; i < transaction_id_max_ - transaction_id_min_; ++i) {
      if (available_transaction_ids_[i]) {
        available_transaction_ids_[i] = false;
        return transaction_id_min_ + i;
      }
    }
    return std::unexpected{
        std::make_error_code(std::errc::resource_unavailable_try_again)};
  }

  auto releaseTransactionID_(uint16_t transaction_id) noexcept -> void {
    if (transaction_id < transaction_id_min_ ||
        transaction_id >= transaction_id_max_) {
      assert(false && "Transaction ID out of range");
      return;
    }
    std::lock_guard<std::mutex> lock(transaction_ids_mtx_);
    available_transaction_ids_[transaction_id - transaction_id_min_] = true;
  }

 public:
  auto poll() noexcept -> void override;

  auto runLoop() noexcept -> void override;

  auto send_(size_t total_size)
      -> std::expected<std::monostate, std::error_code>;

  auto registerOnWrite(std::function<void(Packet)> onWrite) noexcept
      -> void override {
    on_write_callback_ = std::move(onWrite);
  }

  auto registerOnRead(
      std::function<std::vector<uint8_t>(Packet)> onRead) noexcept
      -> void override {
    on_read_callback_ = std::move(onRead);
  }

  auto write(std::shared_ptr<TargetNodeBase> target_node,
             uint32_t memory_address,
             const std::span<const uint8_t> data) noexcept
      -> std::expected<std::monostate, std::error_code> override;

  auto read(std::shared_ptr<TargetNodeBase> target_node,
            uint32_t memory_address, const std::span<uint8_t> data) noexcept
      -> std::expected<std::monostate, std::error_code> override;

  auto writeAsync(std::shared_ptr<TargetNodeBase> target_node,
                  uint32_t memory_address, const std::span<const uint8_t> data,
                  std::function<void(Packet)> on_complete) noexcept
      -> std::future<std::expected<std::monostate, std::error_code>> override;

  auto readAsync(std::shared_ptr<TargetNodeBase> target_node,
                 uint32_t memory_address, uint32_t data_length,
                 std::function<void(Packet)> on_complete) noexcept
      -> std::future<std::expected<std::monostate, std::error_code>> override;

  auto emitTimeCode(uint8_t timecode) noexcept
      -> std::expected<std::monostate, std::error_code> override;
};

};  // namespace spw_rmap::internal
