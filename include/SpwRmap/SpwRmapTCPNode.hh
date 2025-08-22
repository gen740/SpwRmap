#pragma once

#include <cstdint>
#include <cstring>
#include <memory>
#include <print>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "SpwRmap/PacketBuilder.hh"
#include "SpwRmap/PacketParser.hh"
#include "SpwRmap/SpwRmapNodeBase.hh"
#include "SpwRmap/internal/TCPClient.hh"

namespace SpwRmap {

class SpwRmapTCPNode : public SpwRmapNodeBase {
 private:
  std::unique_ptr<internal::TCPClient> tcp_client_;
  std::thread worker_thread_;

  std::string_view ip_address_;
  std::string port_;

  std::unique_ptr<std::vector<uint8_t>> receive_buffer_vec_ = nullptr;
  std::span<uint8_t> recv_buffer_ = {};

  std::unique_ptr<std::vector<uint8_t>> send_buffer_vec_ = nullptr;
  std::span<uint8_t> send_buffer_ = {};

  PacketParser packet_parser_ = {};
  ReadPacketBuilder read_packet_builder_ = {};
  WritePacketBuilder write_packet_builder_ = {};
  uint8_t initiator_logical_address_ = 0xFE;

 public:
  explicit SpwRmapTCPNode(std::string_view ip_address, uint32_t port) noexcept
      : ip_address_(ip_address), port_(std::to_string(port)) {}

 public:
  auto connect(
      std::chrono::microseconds recv_timeout = std::chrono::milliseconds(100),
      std::chrono::microseconds send_timeout = std::chrono::milliseconds(100),
      std::chrono::microseconds connect_timeout = std::chrono::milliseconds(
          100)) -> std::expected<std::monostate, std::error_code>;

  auto setBuffer(size_t send_buf_size, size_t recv_buf_size) -> void;

  auto setBuffer(std::span<uint8_t> send_buffer, std::span<uint8_t> recv_buffer)
      -> void;

  auto setInitiatorLogicalAddress(uint8_t address) -> void {
    initiator_logical_address_ = address;
  }

 private:
  auto recvExact_(std::span<uint8_t> buffer)
      -> std::expected<std::size_t, std::error_code>;

  auto recvAndParseOnePacket() -> std::expected<std::size_t, std::error_code>;

  auto ignoreNBytes(std::size_t n)
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
