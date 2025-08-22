#include "SpwRmap/SpwRmapTCPNode.hh"

#include <algorithm>
#include <print>

namespace SpwRmap {

using namespace std::chrono_literals;

auto SpwRmapTCPNode::connect(std::chrono::microseconds recv_timeout,
                             std::chrono::microseconds send_timeout,
                             std::chrono::microseconds connect_timeout)
    -> std::expected<std::monostate, std::error_code> {
  tcp_client_ = std::make_unique<internal::TCPClient>(ip_address_, port_);
  auto res = tcp_client_->connect(recv_timeout, send_timeout, connect_timeout);
  if (!res.has_value()) {
    tcp_client_->disconnect();
    return std::unexpected{res.error()};
  }
  return {};
}

auto SpwRmapTCPNode::setBuffer(size_t send_buf_size, size_t recv_buf_size)
    -> void {
  receive_buffer_vec_ = std::make_unique<std::vector<uint8_t>>(recv_buf_size);
  recv_buffer_ = std::span<uint8_t>(*receive_buffer_vec_);
  send_buffer_vec_ = std::make_unique<std::vector<uint8_t>>(send_buf_size);
  send_buffer_ = std::span<uint8_t>(*send_buffer_vec_);
  read_packet_builder_.setBuffer(
      send_buffer_.subspan(12));  // First 12 bytes are reserved for header
  write_packet_builder_.setBuffer(
      send_buffer_.subspan(12));  // First 12 bytes are reserved for header
}

auto SpwRmapTCPNode::setBuffer(std::span<uint8_t> send_buffer,
                               std::span<uint8_t> recv_buffer) -> void {
  send_buffer_ = send_buffer;
  recv_buffer_ = recv_buffer;
  read_packet_builder_.setBuffer(
      send_buffer_.subspan(12));  // First 12 bytes are reserved for header
  write_packet_builder_.setBuffer(
      send_buffer_.subspan(12));  // First 12 bytes are reserved for header
}

auto SpwRmapTCPNode::recvExact_(std::span<uint8_t> buffer)
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

auto SpwRmapTCPNode::recvAndParseOnePacket()
    -> std::expected<std::size_t, std::error_code> {
  if (!tcp_client_) {
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

    if (header.at(0) != 0x00 && header.at(0) != 0x01 && header.at(0) != 0x02 &&
        header.at(0) != 0x31) {
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
    if (*dataLength > recv_buffer.size()) {
      return std::unexpected{std::make_error_code(std::errc::no_buffer_space)};
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
          return std::unexpected{std::make_error_code(std::errc::bad_message)};
        }
        std::array<uint8_t, 2> tc{};
        auto res = recvExact_(tc);
        if (!res.has_value()) {
          return std::unexpected(res.error());
        }
        if (tc.at(1) != 0x00) {
          return std::unexpected{std::make_error_code(std::errc::bad_message)};
        }
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

auto SpwRmapTCPNode::ignoreNBytes(std::size_t n)
    -> std::expected<std::size_t, std::error_code> {
  if (!tcp_client_) {
    return std::unexpected{std::make_error_code(std::errc::not_connected)};
  }
  const size_t requested_size = n;
  std::array<uint8_t, 16> ignore_buffer{};
  while (n > ignore_buffer.size()) {
    auto res = tcp_client_->recvSome(ignore_buffer);
    if (!res.has_value()) {
      return std::unexpected{res.error()};
    }
    if (res.value() == 0) {
      return std::unexpected{
          std::make_error_code(std::errc::connection_aborted)};
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

auto SpwRmapTCPNode::write(const TargetNodeBase& target_node,
                           uint32_t memory_address,
                           const std::span<const uint8_t> data) noexcept
    -> std::expected<std::monostate, std::error_code> {
  if (!tcp_client_) {
    return std::unexpected{std::make_error_code(std::errc::not_connected)};
  }
  auto expected_length = target_node.getTargetSpaceWireAddress().size() +
                         (target_node.getReplyAddress().size() + 3) / 4 * 4 +
                         4 + 12 + 1 + data.size();
  if (expected_length > send_buffer_.size()) {
    return std::unexpected{std::make_error_code(std::errc::no_buffer_space)};
  }

  auto res = write_packet_builder_.build({
      .targetSpaceWireAddress = target_node.getTargetSpaceWireAddress(),
      .replyAddress = target_node.getReplyAddress(),
      .targetLogicalAddress = target_node.getTargetLogicalAddress(),
      .initiatorLogicalAddress = initiator_logical_address_,
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
  send_buffer_[4] = static_cast<uint8_t>((total_size >> 56) & 0xFF);
  send_buffer_[5] = static_cast<uint8_t>((total_size >> 48) & 0xFF);
  send_buffer_[6] = static_cast<uint8_t>((total_size >> 40) & 0xFF);
  send_buffer_[7] = static_cast<uint8_t>((total_size >> 32) & 0xFF);
  send_buffer_[8] = static_cast<uint8_t>((total_size >> 24) & 0xFF);
  send_buffer_[9] = static_cast<uint8_t>((total_size >> 16) & 0xFF);
  send_buffer_[10] = static_cast<uint8_t>((total_size >> 8) & 0xFF);
  send_buffer_[11] = static_cast<uint8_t>((total_size >> 0) & 0xFF);
  auto res_send =
      tcp_client_->sendAll(send_buffer_.subspan(0, total_size + 12));
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

auto SpwRmapTCPNode::read(const TargetNodeBase& target_node,
                          uint32_t memory_address,
                          const std::span<uint8_t> data) noexcept
    -> std::expected<std::monostate, std::error_code> {
  if (!tcp_client_) {
    return std::unexpected{std::make_error_code(std::errc::not_connected)};
  }
  auto expected_length = target_node.getTargetSpaceWireAddress().size() +
                         (target_node.getReplyAddress().size() + 3) / 4 * 4 +
                         4 + 12 + 1;
  if (expected_length > send_buffer_.size()) {
    return std::unexpected{std::make_error_code(std::errc::no_buffer_space)};
  }
  auto res = read_packet_builder_.build({
      .targetSpaceWireAddress = target_node.getTargetSpaceWireAddress(),
      .replyAddress = target_node.getReplyAddress(),
      .targetLogicalAddress = target_node.getTargetLogicalAddress(),
      .initiatorLogicalAddress = initiator_logical_address_,
      .transactionID = 0x0123,
      .extendedAddress = 0x00,
      .address = memory_address,
      .dataLength = static_cast<uint32_t>(data.size()),
  });
  if (!res.has_value()) {
    return std::unexpected{res.error()};
  }
  if (send_buffer_.size() < read_packet_builder_.getTotalSize() + 12) {
    return std::unexpected{std::make_error_code(std::errc::no_buffer_space)};
  }
  auto total_size = read_packet_builder_.getTotalSize();
  send_buffer_[0] = 0x00;
  send_buffer_[1] = 0x00;
  send_buffer_[2] = 0x00;
  send_buffer_[3] = 0x00;
  send_buffer_[4] = static_cast<uint8_t>((total_size >> 56) & 0xFF);
  send_buffer_[5] = static_cast<uint8_t>((total_size >> 48) & 0xFF);
  send_buffer_[6] = static_cast<uint8_t>((total_size >> 40) & 0xFF);
  send_buffer_[7] = static_cast<uint8_t>((total_size >> 32) & 0xFF);
  send_buffer_[8] = static_cast<uint8_t>((total_size >> 24) & 0xFF);
  send_buffer_[9] = static_cast<uint8_t>((total_size >> 16) & 0xFF);
  send_buffer_[10] = static_cast<uint8_t>((total_size >> 8) & 0xFF);
  send_buffer_[11] = static_cast<uint8_t>((total_size >> 0) & 0xFF);
  auto res_send = tcp_client_->sendAll(send_buffer_.first(total_size + 12));
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

auto SpwRmapTCPNode::emitTimeCode(uint8_t timecode) noexcept
    -> std::expected<std::monostate, std::error_code> {
  if (!tcp_client_) {
    return std::unexpected{std::make_error_code(std::errc::not_connected)};
  }
  std::array<uint8_t, 14> packet{};
  packet.at(0) = 0x30;
  packet.at(1) = 0x00;  // reserved
  for (size_t i = 2; i < 11; ++i) {
    packet.at(i) = 0x00;  // reserved
  }
  packet.at(11) = 0x02;  // reserved
  packet.at(12) = timecode;
  packet.at(13) = 0x00;
  return tcp_client_->sendAll(packet);
}

}  // namespace SpwRmap
