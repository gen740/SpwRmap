#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <print>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "SpwRmap/PacketBuilder.hh"
#include "SpwRmap/PacketParser.hh"
#include "SpwRmap/SpwRmapBase.hh"
#include "SpwRmap/internal/TCPClient.hh"

namespace SpwRmap {

class SpwRmap : public SpwRmapBase {
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

  std::vector<TargetNode> target_nodes_ = {};

 public:
  explicit SpwRmap(std::string_view ip_address, uint32_t port) noexcept
      : ip_address_(ip_address), port_(std::to_string(port)) {
    target_nodes_.reserve(16);
  }

 private:
  auto initialize_() -> void {
    tcp_client_ = std::make_unique<internal::TCPClient>(ip_address_, port_);

    auto res = tcp_client_->connect();

    int retry_count = 0;
    while (!res.has_value() || retry_count < 3) {
      std::println(stderr, "Failed to connect to SpaceWire interface: {}",
                   res.error().message());
      std::println(stderr, "Retrying in 1 second...");
      std::this_thread::sleep_for(std::chrono::seconds(1));
      retry_count++;
      res = tcp_client_->reconnect();
    }
    if (!res.has_value()) {
      std::println(
          stderr,
          "Failed to connect to SpaceWire interface after 3 retries: {}",
          res.error().message());
      std::terminate();
    }
  }

 public:
  auto initialize(size_t send_buf_size, size_t recv_buf_size) -> void {
    initialize_();
    receive_buffer_vec_ = std::make_unique<std::vector<uint8_t>>(recv_buf_size);
    recv_buffer_ = std::span<uint8_t>(*receive_buffer_vec_);
    send_buffer_vec_ = std::make_unique<std::vector<uint8_t>>(send_buf_size);
    send_buffer_ = std::span<uint8_t>(*send_buffer_vec_);
    read_packet_builder_.setBuffer(
        send_buffer_.subspan(12));  // First 12 bytes are reserved for header
    write_packet_builder_.setBuffer(
        send_buffer_.subspan(12));  // First 12 bytes are reserved for header
  }

  auto initialize(std::span<uint8_t> send_buffer,
                  std::span<uint8_t> recv_buffer) -> void {
    initialize_();
    send_buffer_ = send_buffer;
    recv_buffer_ = recv_buffer;
    read_packet_builder_.setBuffer(
        send_buffer_.subspan(12));  // First 12 bytes are reserved for header
    write_packet_builder_.setBuffer(
        send_buffer_.subspan(12));  // First 12 bytes are reserved for header
  }

 private:
  auto recvExact_(std::span<uint8_t> buffer)
      -> std::expected<std::size_t, std::error_code> {
    if (!tcp_client_) {
      return std::unexpected{std::make_error_code(std::errc::not_connected)};
    }
    size_t total_length = buffer.size();
    while (!buffer.empty()) {
      auto res = tcp_client_->recvSome(buffer);
      if (!res.has_value()) {
        return std::unexpected(res.error());
      }
      if (res.value() == 0) {
        return std::unexpected{
            std::make_error_code(std::errc::connection_aborted)};
      }
      buffer = buffer.subspan(res.value());
    }
    return total_length;
  }

  auto recvAndParseOnePacket() -> std::expected<std::size_t, std::error_code> {
    if (!tcp_client_) {
      return std::unexpected{std::make_error_code(std::errc::not_connected)};
    }
    if (recv_buffer_.size() < 12) {
      return std::unexpected{std::make_error_code(std::errc::no_buffer_space)};
    }
    return recvExact_(recv_buffer_.subspan(0, 12))
        .and_then(
            [this](std::size_t) -> std::expected<std::size_t, std::error_code> {
              uint16_t extra_length = 0;
              uint64_t data_length = 0;
              extra_length = (recv_buffer_[3] << 8) | recv_buffer_[4];
              data_length = (static_cast<uint64_t>(recv_buffer_[5]) << 56 |
                             static_cast<uint64_t>(recv_buffer_[6]) << 48 |
                             static_cast<uint64_t>(recv_buffer_[7]) << 40 |
                             static_cast<uint64_t>(recv_buffer_[8]) << 32 |
                             static_cast<uint64_t>(recv_buffer_[9]) << 24 |
                             static_cast<uint64_t>(recv_buffer_[10]) << 16 |
                             static_cast<uint64_t>(recv_buffer_[11]));
              if (extra_length > 0 || data_length > recv_buffer_.size() - 12) {
                return std::unexpected{
                    std::make_error_code(std::errc::no_buffer_space)};
              }
              return recvExact_(recv_buffer_.subspan(12, data_length));
            })
        .and_then([this](std::size_t data_length)
                      -> std::expected<std::size_t, std::error_code> {
          auto status =
              packet_parser_.parse(recv_buffer_.subspan(0, 12 + data_length));
          if (status != PacketParser::Status::Success) {
            return std::unexpected{make_error_code(status)};
          }
          return 12 + data_length;
        });
  }

 public:
  auto addTargetNode(const TargetNode &target_node) -> void override {
    target_nodes_.push_back(target_node);
  }

  auto addTargetNode(TargetNode &&target_node) -> void override {
    target_nodes_.emplace_back(std::move(target_node));
  }

  auto write(uint8_t logical_address, uint32_t memory_address,
             const std::span<const uint8_t> data)
      -> std::expected<std::monostate, std::error_code> override {
    if (!tcp_client_) {
      return std::unexpected{std::make_error_code(std::errc::not_connected)};
    }
    auto target_node = std::ranges::find_if(
        target_nodes_, [logical_address](const TargetNode &node) {
          return node.logical_address == logical_address;
        });
    if (target_node == target_nodes_.end()) {
      return std::unexpected{std::make_error_code(std::errc::invalid_argument)};
    }
    auto expected_length = target_node->target_spacewire_address.size() +
                           (target_node->reply_address.size() + 3) / 4 * 4 + 4 +
                           12 + 1 + data.size();
    if (expected_length > send_buffer_.size()) {
      return std::unexpected{std::make_error_code(std::errc::no_buffer_space)};
    }

    auto res = write_packet_builder_.build({
        .targetSpaceWireAddress = target_node->target_spacewire_address,
        .replyAddress = target_node->reply_address,
        .targetLogicalAddress = target_node->logical_address,
        .transactionID = 0x0123,
        .extendedAddress = 0x00,
        .address = memory_address,
        .data = data,
    });
    if (!res.has_value()) {
      return std::unexpected{res.error()};
    }
    auto total_size = write_packet_builder_.getTotalSize();
    send_buffer_[0] = 0x00;
    send_buffer_[1] = 0x00;
    send_buffer_[2] = 0x00;
    send_buffer_[3] = 0x00;
    send_buffer_[4] = 0x00;
    send_buffer_[5] = 0x00;
    send_buffer_[6] = 0x00;
    send_buffer_[7] = 0x00;
    send_buffer_[8] = static_cast<uint8_t>((total_size >> 24) & 0xFF);
    send_buffer_[9] = static_cast<uint8_t>((total_size >> 16) & 0xFF);
    send_buffer_[10] = static_cast<uint8_t>((total_size >> 8) & 0xFF);
    send_buffer_[11] = static_cast<uint8_t>((total_size >> 0) & 0xFF);
    auto res_send = tcp_client_->sendAll(send_buffer_.subspan(0, total_size));
    if (!res_send.has_value()) {
      return std::unexpected{res_send.error()};
    }

    size_t max_trial_count = 10;
    size_t trial_count = 0;
    do {
      auto recvRes = recvAndParseOnePacket();
      if (!recvRes.has_value()) {
        return std::unexpected{recvRes.error()};
      }
      trial_count++;
      if (trial_count >= max_trial_count) {
        return std::unexpected{std::make_error_code(std::errc::timed_out)};
      }
    } while (packet_parser_.getPacket().type != PacketType::WriteReply &&
             packet_parser_.getPacket().transactionID != 0x0123);
    return {};
  }

  auto read(uint8_t logical_address, uint32_t memory_address,
            const std::span<uint8_t> data)
      -> std::expected<std::monostate, std::error_code> override {
    if (!tcp_client_) {
      return std::unexpected{std::make_error_code(std::errc::not_connected)};
    }
    auto target_node = std::ranges::find_if(
        target_nodes_, [logical_address](const TargetNode &node) {
          return node.logical_address == logical_address;
        });
    if (target_node == target_nodes_.end()) {
      return std::unexpected{std::make_error_code(std::errc::invalid_argument)};
    }
    auto expected_length = target_node->target_spacewire_address.size() +
                           (target_node->reply_address.size() + 3) / 4 * 4 + 4 +
                           12 + 1;
    if (expected_length > send_buffer_.size()) {
      return std::unexpected{std::make_error_code(std::errc::no_buffer_space)};
    }
    auto res = read_packet_builder_.build({
        .targetSpaceWireAddress = target_node->target_spacewire_address,
        .replyAddress = target_node->reply_address,
        .targetLogicalAddress = target_node->logical_address,
        .transactionID = 0x0123,
        .extendedAddress = 0x00,
        .address = memory_address,
        .dataLength = static_cast<uint32_t>(data.size()),
    });
    if (!res.has_value()) {
      return std::unexpected{res.error()};
    }
    auto total_size = write_packet_builder_.getTotalSize();
    send_buffer_[0] = 0x00;
    send_buffer_[1] = 0x00;
    send_buffer_[2] = 0x00;
    send_buffer_[3] = 0x00;
    send_buffer_[4] = 0x00;
    send_buffer_[5] = 0x00;
    send_buffer_[6] = 0x00;
    send_buffer_[7] = 0x00;
    send_buffer_[8] = static_cast<uint8_t>((total_size >> 24) & 0xFF);
    send_buffer_[9] = static_cast<uint8_t>((total_size >> 16) & 0xFF);
    send_buffer_[10] = static_cast<uint8_t>((total_size >> 8) & 0xFF);
    send_buffer_[11] = static_cast<uint8_t>((total_size >> 0) & 0xFF);
    auto res_send = tcp_client_->sendAll(send_buffer_.subspan(0, total_size));
    if (!res_send.has_value()) {
      return std::unexpected{res_send.error()};
    }

    size_t max_trial_count = 10;
    size_t trial_count = 0;
    do {
      auto recvRes = recvAndParseOnePacket();
      if (!recvRes.has_value()) {
        return std::unexpected{recvRes.error()};
      }
      trial_count++;
      if (trial_count >= max_trial_count) {
        return std::unexpected{std::make_error_code(std::errc::timed_out)};
      }
    } while (packet_parser_.getPacket().type != PacketType::ReadReply &&
             packet_parser_.getPacket().transactionID != 0x0123);
    if (packet_parser_.getPacket().data.size() == data.size()) {
      std::copy(packet_parser_.getPacket().data.begin(),
                packet_parser_.getPacket().data.end(), data.begin());
    } else {
      return std::unexpected{std::make_error_code(std::errc::invalid_argument)};
    }
    return {};
  }

  auto emitTimeCode(uint8_t timecode)
      -> std::expected<std::monostate, std::error_code> override {
    if (!tcp_client_) {
      return std::unexpected{std::make_error_code(std::errc::not_connected)};
    }
    std::array<uint8_t, 14> packet{};
    packet.at(0) = 0x30;
    packet.at(1) = 0x00;  // reserved
    for (size_t i = 2; i < 12; ++i) {
      packet.at(i) = 0x00;  // reserved
    }
    packet.at(12) = timecode;
    packet.at(13) = 0x00;
    return tcp_client_->sendAll(packet);
  }
};

};  // namespace SpwRmap
