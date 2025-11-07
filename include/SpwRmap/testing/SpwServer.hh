#pragma once

#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <expected>
#include <random>
#include <span>
#include <string_view>
#include <system_error>
#include <vector>

#include "SpwRmap/PacketBuilder.hh"
#include "SpwRmap/PacketParser.hh"
#include "SpwRmap/internal/TCPServer.hh"

using SpwRmap::PacketBuilderBase;
using SpwRmap::PacketParser;
using SpwRmap::ReadReplyPacketBuilder;
using SpwRmap::ReadReplyPacketConfig;
using SpwRmap::status_code_category;
using SpwRmap::WriteReplyPacketBuilder;
using SpwRmap::WriteReplyPacketConfig;
using SpwRmap::internal::TCPServer;

namespace SpwRmap::testing {

using namespace std::chrono_literals;

class MemoryDevice final {
 private:
  static constexpr std::size_t memSize_ = 1 << 20;  // 1 MiB
  std::array<uint8_t, memSize_> mem_{};

 public:
  MemoryDevice() = default;

  [[nodiscard]] auto read(std::uint32_t addr, std::uint32_t len) noexcept
      -> std::expected<std::span<const uint8_t>, std::error_code> {
    if (static_cast<std::size_t>(addr) + static_cast<std::size_t>(len) >
        mem_.size()) {
      return std::unexpected{
          std::make_error_code(std::errc::result_out_of_range)};
    }
    return std::span<const uint8_t>(mem_).subspan(addr, len);
  }

  [[nodiscard]] auto write(std::uint32_t addr,
                           std::span<const uint8_t> data) noexcept
      -> std::expected<std::monostate, std::error_code> {
    if (static_cast<std::size_t>(addr) + data.size() > mem_.size()) {
      return std::unexpected{
          std::make_error_code(std::errc::result_out_of_range)};
    }
    std::ranges::copy(data, mem_.begin() + addr);
    return {};
  }
};

static inline auto calculateDataLength(const std::span<const uint8_t> header,
                                       size_t max_size) noexcept
    -> std::expected<size_t, std::error_code> {
  if (header.size() < 12) {
    return std::unexpected{std::make_error_code(std::errc::invalid_argument)};
  }
  uint16_t extra_length = (static_cast<uint16_t>(header[2]) << 8) |
                          (static_cast<uint16_t>(header[3]) << 0);
  uint64_t data_length = ((static_cast<uint64_t>(header[4]) << 56) |
                          (static_cast<uint64_t>(header[5]) << 48) |
                          (static_cast<uint64_t>(header[6]) << 40) |
                          (static_cast<uint64_t>(header[7]) << 32) |
                          (static_cast<uint64_t>(header[8]) << 24) |
                          (static_cast<uint64_t>(header[9]) << 16) |
                          (static_cast<uint64_t>(header[10]) << 8) |
                          (static_cast<uint64_t>(header[11]) << 0));
  if (extra_length > 0 || data_length > max_size) {
    return std::unexpected{std::make_error_code(std::errc::no_buffer_space)};
  }
  return data_length;
}

class SSDTP2Server {
 private:
  std::unique_ptr<TCPServer> tcp_server_;
  std::atomic_flag running_{true};
  std::vector<uint8_t> recv_buffer_vec_{};  // 1 MiB
  std::span<uint8_t> recv_buffer_{};
  PacketParser packet_parser_ = {};
  uint8_t timecode_ = 0;
  MemoryDevice memory_device_;
  ReadReplyPacketBuilder read_reply_builder_ = {};
  WriteReplyPacketBuilder write_reply_builder_ = {};

  std::mt19937 random_engine_{};

 public:
  SSDTP2Server(std::string_view bind_address, std::string_view port)
      : tcp_server_(std::make_unique<TCPServer>(bind_address, port)),
        random_engine_(std::random_device{}()) {
    recv_buffer_vec_.resize(1 << 20);  // 1 MiB
    recv_buffer_ = std::span<uint8_t>(recv_buffer_vec_);
  }

  auto stop() noexcept -> void { running_.clear(std::memory_order_release); }

  auto run() noexcept -> std::expected<std::monostate, std::error_code> {
    if (auto r = tcp_server_->accept_once(1s, 1s); !r.has_value()) {
      return std::unexpected{r.error()};
    }
    while (running_.test(std::memory_order_acquire)) {
      auto res = recvAndParseOnePacket_();
      if (!res.has_value()) {
        return std::unexpected{res.error()};
      }

      switch (packet_parser_.getPacket().type) {
        case PacketType::Read: {
          auto memory_data =
              memory_device_.read(packet_parser_.getPacket().address,
                                  packet_parser_.getPacket().dataLength);
          if (!memory_data.has_value()) {
            return std::unexpected{memory_data.error()};
          }
          auto config = ReadReplyPacketConfig{
              .replyAddress = packet_parser_.getPacket().replyAddress,
              .initiatorLogicalAddress =
                  packet_parser_.getPacket().initiatorLogicalAddress,
              .status = 0x00,
              .targetLogicalAddress =
                  packet_parser_.getPacket().targetLogicalAddress,
              .transactionID = packet_parser_.getPacket().transactionID,
              .data = *memory_data,
          };
          std::vector<uint8_t> buffer(read_reply_builder_.getTotalSize(config));
          auto res = read_reply_builder_.build(config, buffer);
          if (!res.has_value()) {
            return std::unexpected{res.error()};
          }
          {
            auto res = send_ssdtp2(buffer);
            if (!res.has_value()) {
              return std::unexpected{res.error()};
            }
          }
          break;
        }
        case PacketType::Write: {
          auto memory_res =
              memory_device_.write(packet_parser_.getPacket().address,
                                   packet_parser_.getPacket().data);
          if (!memory_res.has_value()) {
            return std::unexpected{memory_res.error()};
          }
          auto config = WriteReplyPacketConfig{
              .replyAddress = packet_parser_.getPacket().replyAddress,
              .initiatorLogicalAddress =
                  packet_parser_.getPacket().initiatorLogicalAddress,
              .status = 0x00,
              .targetLogicalAddress =
                  packet_parser_.getPacket().targetLogicalAddress,
              .transactionID = packet_parser_.getPacket().transactionID,
              .verifyMode = true,  // Always true for this server
          };
          std::vector<uint8_t> buffer(
              write_reply_builder_.getTotalSize(config));

          auto res = write_reply_builder_.build(config, buffer);
          if (!res.has_value()) {
            return std::unexpected{res.error()};
          }
          {
            auto res = send_ssdtp2(buffer);
            if (!res.has_value()) {
              return std::unexpected{res.error()};
            }
          }
          break;
        }
        default: {
          return std::unexpected{std::make_error_code(std::errc::bad_message)};
        }
      }
    }
    return {};
  }

  auto recvExact_(std::span<uint8_t> buffer) noexcept
      -> std::expected<std::size_t, std::error_code> {
    if (!tcp_server_) {
      return std::unexpected{std::make_error_code(std::errc::not_connected)};
    }
    size_t total_length = buffer.size();
    while (!buffer.empty()) {
      auto res = tcp_server_->recvSome(buffer);
      if (!res.has_value()) {
        return std::unexpected(res.error());
      }
      if (res.value() == 0) {
        return std::unexpected{
            std::make_error_code(std::errc::connection_reset)};
      }
      buffer = buffer.subspan(res.value());
    }
    return total_length;
  }

  auto ignoreNBytes(std::size_t n)
      -> std::expected<std::size_t, std::error_code> {
    if (!tcp_server_) {
      return std::unexpected{std::make_error_code(std::errc::not_connected)};
    }
    const size_t requested_size = n;
    std::array<uint8_t, 16> ignore_buffer{};
    while (n > ignore_buffer.size()) {
      auto res = tcp_server_->recvSome(ignore_buffer);
      if (!res.has_value()) {
        return std::unexpected{res.error()};
      }
      if (res.value() == 0) {
        return std::unexpected{
            std::make_error_code(std::errc::connection_reset)};
      }
      n -= res.value();
    }
    if (n > 0) {
      auto res = recvExact_(std::span(ignore_buffer).first(n));
      if (!res.has_value()) {
        return std::unexpected{res.error()};
      }
    }
    return requested_size;
  }

  auto recvAndParseOnePacket_() noexcept
      -> std::expected<size_t, std::error_code> {
    if (!tcp_server_) {
      return std::unexpected{std::make_error_code(std::errc::not_connected)};
    }
    auto recv_buffer = recv_buffer_;
    size_t total_size = 0;
    auto eof = false;
    while (!eof) {
      std::array<uint8_t, 12> header{};
      auto res = recvExact_(header);
      if (!res.has_value()) {
        return std::unexpected(res.error());
      }

      if (header.at(0) != 0x00 && header.at(0) != 0x01 &&
          header.at(0) != 0x02 && header.at(0) != 0x31) {
        return std::unexpected{std::make_error_code(std::errc::bad_message)};
      }
      if (header.at(1) != 0x00) {
        return std::unexpected{std::make_error_code(std::errc::bad_message)};
      }

      auto dataLength = calculateDataLength(header, recv_buffer.size());
      if (!dataLength.has_value()) {
        return std::unexpected(dataLength.error());
      }
      if (*dataLength == 0) {
        return std::unexpected{std::make_error_code(std::errc::bad_message)};
      }

      switch (header.at(0)) {
        case 0x00: {
          auto res = recvExact_(recv_buffer.first(*dataLength));
          if (!res.has_value()) {
            return std::unexpected(res.error());
          }
          total_size += *res;
          eof = true;
        } break;
        case 0x01: {
          auto res = ignoreNBytes(*dataLength);
          if (!res.has_value()) {
            return std::unexpected(res.error());
          }
          return std::unexpected{std::make_error_code(std::errc::bad_message)};
        } break;
        case 0x02: {
          auto res = recvExact_(recv_buffer.first(*dataLength));
          if (!res.has_value()) {
            return std::unexpected(res.error());
          }
          total_size += *res;
          recv_buffer = recv_buffer.subspan(*dataLength);
        } break;
        case 0x31: {
          // Timecode packet
          if (header.at(2) != 0x00 || header.at(3) != 0x00 ||
              header.at(4) != 0x00 || header.at(5) != 0x00 ||
              header.at(6) != 0x00 || header.at(7) != 0x00 ||
              header.at(8) != 0x00 || header.at(9) != 0x00 ||
              header.at(10) != 0x00 || header.at(11) != 0x02) {
            return std::unexpected{
                std::make_error_code(std::errc::bad_message)};
          }
          std::array<uint8_t, 2> tc{};
          auto res = recvExact_(tc);
          if (!res.has_value()) {
            return std::unexpected(res.error());
          }
          if (tc.at(1) != 0x00 || (tc[0] & 0xC0) != 0x00) {
            return std::unexpected{
                std::make_error_code(std::errc::bad_message)};
          }
          timecode_ = tc.at(0) & 0x3F;
        } break;
        default:
          return std::unexpected{std::make_error_code(std::errc::bad_message)};
      }
    }
    auto status = packet_parser_.parse(recv_buffer_.first(total_size));
    if (status != PacketParser::Status::Success) {
      return std::unexpected{make_error_code(status)};
    }
    return total_size;
  }

  auto send_ssdtp2(std::span<const uint8_t> payload) noexcept
      -> std::expected<std::monostate, std::error_code> {
    while (payload.size() != 0) {
      auto packet_to_send =
          std::uniform_int_distribution<size_t>(5, 400)(random_engine_);
      packet_to_send = std::min(packet_to_send, payload.size());
      uint8_t flag = 0x00;
      if (packet_to_send == payload.size()) {
        flag = 0x00;  // EOP
      } else {
        flag = 0x02;  // continue
      }

      auto p = payload.first(packet_to_send);
      payload = payload.subspan(packet_to_send);

      std::array<uint8_t, 12> header{};
      header[0] = flag;
      header[1] = 0x00;
      header[2] = 0x00;
      header[3] = 0x00;
      header[4] = static_cast<uint8_t>((p.size() >> 56) & 0xFF);
      header[5] = static_cast<uint8_t>((p.size() >> 48) & 0xFF);
      header[6] = static_cast<uint8_t>((p.size() >> 40) & 0xFF);
      header[7] = static_cast<uint8_t>((p.size() >> 32) & 0xFF);
      header[8] = static_cast<uint8_t>((p.size() >> 24) & 0xFF);
      header[9] = static_cast<uint8_t>((p.size() >> 16) & 0xFF);
      header[10] = static_cast<uint8_t>((p.size() >> 8) & 0xFF);
      header[11] = static_cast<uint8_t>((p.size() >> 0) & 0xFF);
      if (auto r = tcp_server_->sendAll(header); !r.has_value()) {
        return std::unexpected{r.error()};
      }
      if (auto r = tcp_server_->sendAll(p); !r.has_value()) {
        return std::unexpected{r.error()};
      }
    }
    return {};
  }

  [[nodiscard]] auto getTimecode() const noexcept -> uint8_t {
    return timecode_;
  }
};

}  // namespace SpwRmap::testing
