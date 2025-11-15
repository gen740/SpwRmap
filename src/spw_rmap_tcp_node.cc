#include "spw_rmap/spw_rmap_tcp_node.hh"

#include <algorithm>
#include <condition_variable>
#include <iostream>

namespace spw_rmap {

using namespace std::chrono_literals;

auto SpwRmapTCPNode::connect(std::chrono::microseconds recv_timeout,
                             std::chrono::microseconds send_timeout,
                             std::chrono::microseconds connect_timeout)
    -> std::expected<std::monostate, std::error_code> {
  std::cout << "Connecting to " << ip_address_ << ":" << port_ << "...\n";
  tcp_client_ = std::make_unique<internal::TCPClient>(ip_address_, port_);
  auto res = tcp_client_->connect(recv_timeout, send_timeout, connect_timeout);
  if (!res.has_value()) {
    tcp_client_->disconnect();
    return std::unexpected{res.error()};
  }
  return {};
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

static inline auto calculateDataLength(
    const std::span<const uint8_t> header) noexcept
    -> std::expected<size_t, std::error_code> {
  if (header.size() < 12) {
    return std::unexpected{std::make_error_code(std::errc::invalid_argument)};
  }
  std::ignore /* extra_length */ = (static_cast<uint16_t>(header[2]) << 8) |
                                   (static_cast<uint16_t>(header[3]) << 0);
  uint64_t data_length = ((static_cast<uint64_t>(header[4]) << 56) |
                          (static_cast<uint64_t>(header[5]) << 48) |
                          (static_cast<uint64_t>(header[6]) << 40) |
                          (static_cast<uint64_t>(header[7]) << 32) |
                          (static_cast<uint64_t>(header[8]) << 24) |
                          (static_cast<uint64_t>(header[9]) << 16) |
                          (static_cast<uint64_t>(header[10]) << 8) |
                          (static_cast<uint64_t>(header[11]) << 0));
  return data_length;
}

auto SpwRmapTCPNode::recvAndParseOnePacket_()
    -> std::expected<std::size_t, std::error_code> {
  if (!tcp_client_) {
    return std::unexpected{std::make_error_code(std::errc::not_connected)};
  }
  size_t total_size = 0;
  auto eof = false;
  auto recv_buffer = std::span(recv_buf_);
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

    auto dataLength = calculateDataLength(header);
    if (!dataLength.has_value()) {
      return std::unexpected(dataLength.error());
    }
    if (*dataLength == 0) {
      return std::unexpected{std::make_error_code(std::errc::bad_message)};
    }
    if (*dataLength > recv_buffer.size()) {
      if (buffer_policy_ == BufferPolicy::Fixed) {
        return std::unexpected{
            std::make_error_code(std::errc::no_buffer_space)};
      } else {
        recv_buf_.resize(total_size + *dataLength);
        recv_buffer = std::span(recv_buf_).subspan(total_size);
      }
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
        auto res = ignoreNBytes_(*dataLength);
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
  auto status = packet_parser_.parse(std::span(recv_buf_).first(total_size));
  if (status != PacketParser::Status::Success) {
    return std::unexpected{make_error_code(status)};
  }
  return total_size;
}

auto SpwRmapTCPNode::ignoreNBytes_(std::size_t n)
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

auto SpwRmapTCPNode::runLoop() noexcept -> void {
  running_.store(true);

  while (running_.load()) {
    auto res = recvAndParseOnePacket_();
    if (!res.has_value()) {
      std::cerr << "Error in receiving/parsing packet: "
                << res.error().message() << "\n";
      continue;
    }

    auto& packet = packet_parser_.getPacket();

    switch (packet.type) {
      case PacketType::ReadReply:
      case PacketType::WriteReply: {
        if (packet.transactionID < transaction_id_min_ ||
            packet.transactionID >= transaction_id_max_) {
          std::cerr << "Received packet with out-of-range Transaction ID: "
                    << packet.transactionID << "\n";
          continue;
        }
        auto res = recv_thread_pool_.post([this, packet]() noexcept -> void {
          std::lock_guard<std::mutex> lock(
              *reply_callback_mtx_[packet.transactionID - transaction_id_min_]);
          if (reply_callback_[packet.transactionID - transaction_id_min_]) {
            reply_callback_[packet.transactionID - transaction_id_min_](packet);
            reply_callback_[packet.transactionID - transaction_id_min_] =
                nullptr;
          } else {
            std::cerr << "No callback registered for Transaction ID: "
                      << packet.transactionID << "\n";
          }
        });
        if (!res.has_value()) {
          std::cerr << "Failed to post callback to thread pool: "
                    << res.error().message() << "\n";
        }
        break;
      }
      default:
        std::cout << "Received Other Packet Type: "
                  << static_cast<uint8_t>(packet.type) << "\n";
        break;
    }
  }
}

auto SpwRmapTCPNode::sendWritePacket_(
    std::shared_ptr<TargetNodeBase> target_node, uint16_t transaction_id,
    uint32_t memory_address, const std::span<const uint8_t> data) noexcept
    -> std::expected<std::monostate, std::error_code> {
  if (!tcp_client_) {
    return std::unexpected{std::make_error_code(std::errc::not_connected)};
  }
  auto expected_length = target_node->getTargetSpaceWireAddress().size() +
                         (target_node->getReplyAddress().size() + 3) / 4 * 4 +
                         4 + 12 + 1 + data.size();
  auto send_buffer = std::span(send_buf_);
  if (expected_length > send_buffer.size()) {
    if (buffer_policy_ == BufferPolicy::Fixed) {
      return std::unexpected{std::make_error_code(std::errc::no_buffer_space)};
    } else {
      send_buf_.resize(expected_length);
      send_buffer = std::span(send_buf_);
    }
  }

  auto config = WritePacketConfig{
      .targetSpaceWireAddress = target_node->getTargetSpaceWireAddress(),
      .replyAddress = target_node->getReplyAddress(),
      .targetLogicalAddress = target_node->getTargetLogicalAddress(),
      .initiatorLogicalAddress = initiator_logical_address_,
      .transactionID = transaction_id,
      .extendedAddress = 0x00,
      .address = memory_address,
      .data = data,
  };

  auto res = write_packet_builder_.build(config, send_buffer.subspan(12));
  if (!res.has_value()) {
    return std::unexpected{res.error()};
  }
  auto total_size = write_packet_builder_.getTotalSize(config);
  send_buffer[0] = 0x00;
  send_buffer[1] = 0x00;
  send_buffer[2] = 0x00;
  send_buffer[3] = 0x00;
  send_buffer[4] = static_cast<uint8_t>((total_size >> 56) & 0xFF);
  send_buffer[5] = static_cast<uint8_t>((total_size >> 48) & 0xFF);
  send_buffer[6] = static_cast<uint8_t>((total_size >> 40) & 0xFF);
  send_buffer[7] = static_cast<uint8_t>((total_size >> 32) & 0xFF);
  send_buffer[8] = static_cast<uint8_t>((total_size >> 24) & 0xFF);
  send_buffer[9] = static_cast<uint8_t>((total_size >> 16) & 0xFF);
  send_buffer[10] = static_cast<uint8_t>((total_size >> 8) & 0xFF);
  send_buffer[11] = static_cast<uint8_t>((total_size >> 0) & 0xFF);
  return tcp_client_->sendAll(send_buffer.subspan(0, total_size + 12));
}

auto SpwRmapTCPNode::sendReadPacket_(
    std::shared_ptr<TargetNodeBase> target_node, uint16_t transaction_id,
    uint32_t memory_address, uint32_t data_length) noexcept
    -> std::expected<std::monostate, std::error_code> {
  if (!tcp_client_) {
    return std::unexpected{std::make_error_code(std::errc::not_connected)};
  }
  auto expected_length = target_node->getTargetSpaceWireAddress().size() +
                         (target_node->getReplyAddress().size() + 3) / 4 * 4 +
                         4 + 12 + 1;
  auto send_buffer = std::span(send_buf_);
  if (expected_length > send_buffer.size()) {
    if (buffer_policy_ == BufferPolicy::Fixed) {
      return std::unexpected{std::make_error_code(std::errc::no_buffer_space)};
    } else {
      send_buf_.resize(expected_length);
      send_buffer = std::span(send_buf_);
    }
  }

  auto config = ReadPacketConfig{
      .targetSpaceWireAddress = target_node->getTargetSpaceWireAddress(),
      .replyAddress = target_node->getReplyAddress(),
      .targetLogicalAddress = target_node->getTargetLogicalAddress(),
      .initiatorLogicalAddress = initiator_logical_address_,
      .transactionID = transaction_id,
      .extendedAddress = 0x00,
      .address = memory_address,
      .dataLength = data_length,
  };

  auto res = read_packet_builder_.build(config, send_buffer.subspan(12));
  if (!res.has_value()) {
    return std::unexpected{res.error()};
  }
  if (send_buffer.size() < read_packet_builder_.getTotalSize(config) + 12) {
    if (buffer_policy_ == BufferPolicy::Fixed) {
      return std::unexpected{std::make_error_code(std::errc::no_buffer_space)};
    } else {
      send_buf_.resize(read_packet_builder_.getTotalSize(config) + 12);
      send_buffer = std::span(send_buf_);
    }
  }
  auto total_size = read_packet_builder_.getTotalSize(config);
  send_buffer[0] = 0x00;
  send_buffer[1] = 0x00;
  send_buffer[2] = 0x00;
  send_buffer[3] = 0x00;
  send_buffer[4] = static_cast<uint8_t>((total_size >> 56) & 0xFF);
  send_buffer[5] = static_cast<uint8_t>((total_size >> 48) & 0xFF);
  send_buffer[6] = static_cast<uint8_t>((total_size >> 40) & 0xFF);
  send_buffer[7] = static_cast<uint8_t>((total_size >> 32) & 0xFF);
  send_buffer[8] = static_cast<uint8_t>((total_size >> 24) & 0xFF);
  send_buffer[9] = static_cast<uint8_t>((total_size >> 16) & 0xFF);
  send_buffer[10] = static_cast<uint8_t>((total_size >> 8) & 0xFF);
  send_buffer[11] = static_cast<uint8_t>((total_size >> 0) & 0xFF);
  auto res_send = tcp_client_->sendAll(send_buffer.first(total_size + 12));

  if (!res_send.has_value()) {
    return std::unexpected{res_send.error()};
  }
  return {};
}

auto SpwRmapTCPNode::writeAsync(                  //
    std::shared_ptr<TargetNodeBase> target_node,  //
    uint32_t memory_address,                      //
    const std::span<const uint8_t> data,          //
    std::function<void(Packet)> on_complete) noexcept
    -> std::future<std::expected<std::monostate, std::error_code>> {
  auto promise = std::make_shared<
      std::promise<std::expected<std::monostate, std::error_code>>>();
  auto future = promise->get_future();

  uint16_t transaction_id = 0;
  {
    auto transaction_id_res = getAvailableTransactionID_();
    if (!transaction_id_res.has_value()) {
      promise->set_value(std::unexpected{transaction_id_res.error()});
      return future;
    }
    transaction_id = transaction_id_res.value();
  }

  auto res =
      sendWritePacket_(target_node, transaction_id, memory_address, data);
  if (!res.has_value()) {
    promise->set_value(std::unexpected{res.error()});
    releaseTransactionID_(transaction_id);
    return future;
  }

  std::lock_guard<std::mutex> lock(
      *(reply_callback_mtx_[transaction_id - transaction_id_min_]));
  reply_callback_[transaction_id - transaction_id_min_] =
      [this, on_complete = std::move(on_complete), promise,
       transaction_id](const Packet& packet) mutable noexcept -> void {
    on_complete(packet);
    promise->set_value({});
    releaseTransactionID_(transaction_id);
  };
  return future;
}

auto SpwRmapTCPNode::write(
    [[maybe_unused]] std::shared_ptr<TargetNodeBase> target_node,
    [[maybe_unused]] uint32_t memory_address,
    [[maybe_unused]] const std::span<const uint8_t> data) noexcept
    -> std::expected<std::monostate, std::error_code> {
  return writeAsync(target_node, memory_address, data,
                    [](const Packet&) noexcept -> void {})
      .get();
}

auto SpwRmapTCPNode::readAsync(std::shared_ptr<TargetNodeBase> target_node,
                               uint32_t memory_address, uint32_t data_length,
                               std::function<void(Packet)> on_complete) noexcept
    -> std::future<std::expected<std::monostate, std::error_code>> {
  auto promise = std::make_shared<
      std::promise<std::expected<std::monostate, std::error_code>>>();
  auto future = promise->get_future();

  uint16_t transaction_id = 0;
  {
    auto transaction_id_res = getAvailableTransactionID_();
    if (!transaction_id_res.has_value()) {
      promise->set_value(std::unexpected{transaction_id_res.error()});
      return future;
    }
    transaction_id = transaction_id_res.value();
  }

  auto res =
      sendReadPacket_(target_node, transaction_id, memory_address, data_length);
  if (!res.has_value()) {
    promise->set_value(std::unexpected{res.error()});
    releaseTransactionID_(transaction_id);
    return future;
  }

  std::lock_guard<std::mutex> lock(
      *(reply_callback_mtx_[transaction_id - transaction_id_min_]));
  reply_callback_[transaction_id - transaction_id_min_] =
      [this, on_complete = std::move(on_complete), promise,
       transaction_id](const Packet& packet) mutable noexcept -> void {
    on_complete(packet);
    promise->set_value({});
    releaseTransactionID_(transaction_id);
  };
  return future;
}

auto SpwRmapTCPNode::read(
    [[maybe_unused]] std::shared_ptr<TargetNodeBase> target_node,
    [[maybe_unused]] uint32_t memory_address,
    [[maybe_unused]] const std::span<uint8_t> data) noexcept
    -> std::expected<std::monostate, std::error_code> {
  return readAsync(target_node, memory_address, data.size(),
                   [data](const Packet& packet) noexcept -> void {
                     std::copy_n(packet.data.data(), data.size(), data.data());
                   })
      .get();
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

}  // namespace spw_rmap
