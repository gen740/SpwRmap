#pragma once

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "SpwRmap/PacketBuilder.hh"
#include "SpwRmap/PacketParser.hh"
#include "SpwRmap/SpwRmapNodeBase.hh"
#include "SpwRmap/internal/TCPClient.hh"

namespace SpwRmap {

using namespace std::chrono_literals;

enum class BufferPolicy : uint8_t {
  Fixed,       // Fixed size
  AutoResize,  // Auto resize if needed
};

struct SpwRmapTCPNodeConfig {
  std::string_view ip_address;
  uint32_t port;
  size_t send_buffer_size = 4096;
  size_t recv_buffer_size = 4096;
  BufferPolicy buffer_policy = BufferPolicy::AutoResize;
};

class SpwRmapTCPNode : public SpwRmapNodeBase {
 private:
  std::unique_ptr<internal::TCPClient> tcp_client_;
  std::thread worker_thread_;

  std::string_view ip_address_;
  std::string port_;

  std::pmr::vector<uint8_t> recv_buf_ = {};
  std::pmr::vector<uint8_t> send_buf_ = {};

  PacketParser packet_parser_ = {};
  ReadPacketBuilder read_packet_builder_ = {};
  WritePacketBuilder write_packet_builder_ = {};
  uint8_t initiator_logical_address_ = 0xFE;
  BufferPolicy buffer_policy_ = BufferPolicy::AutoResize;

 public:
  explicit SpwRmapTCPNode(SpwRmapTCPNodeConfig config,
                          std::pmr::memory_resource* mem_res =
                              std::pmr::get_default_resource()) noexcept
      : ip_address_(config.ip_address),
        port_(std::to_string(config.port)),
        recv_buf_(mem_res),
        send_buf_(mem_res),
        buffer_policy_(config.buffer_policy) {
    recv_buf_.resize(config.recv_buffer_size);
    send_buf_.resize(config.send_buffer_size);
  }

 public:
  auto connect(std::chrono::microseconds recv_timeout = 100ms,
               std::chrono::microseconds send_timeout = 100ms,
               std::chrono::microseconds connect_timeout = 100ms)
      -> std::expected<std::monostate, std::error_code>;

  auto setInitiatorLogicalAddress(uint8_t address) -> void {
    initiator_logical_address_ = address;
  }

 private:
  auto recvExact_(std::span<uint8_t> buffer)
      -> std::expected<std::size_t, std::error_code>;

  auto recvAndParseOnePacket_() -> std::expected<std::size_t, std::error_code>;

  auto ignoreNBytes_(std::size_t n)
      -> std::expected<std::size_t, std::error_code>;

 public:
  auto write(const TargetNodeBase& target_node, uint32_t memory_address,
             const std::span<const uint8_t> data) noexcept
      -> std::expected<std::monostate, std::error_code> override;

  auto read(const TargetNodeBase& target_node, uint32_t memory_address,
            const std::span<uint8_t> data) noexcept
      -> std::expected<std::monostate, std::error_code> override;

  auto emitTimeCode(uint8_t timecode) noexcept
      -> std::expected<std::monostate, std::error_code> override;
};

};  // namespace SpwRmap
