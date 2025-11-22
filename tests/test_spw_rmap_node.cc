#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <expected>
#include <memory>
#include <mutex>
#include <span>
#include <vector>

#include "spw_rmap/internal/spw_rmap_tcp_node_impl.hh"
#include "spw_rmap/packet_builder.hh"
#include "spw_rmap/target_node.hh"

namespace {

class MockBackend {
 public:
  MockBackend(std::string ip, std::string port)
      : ip_address_(std::move(ip)), port_(std::move(port)) {}

  auto getIpAddress() const noexcept -> const std::string& {
    return ip_address_;
  }

  auto setIpAddress(std::string ip_address) noexcept -> void {
    ip_address_ = std::move(ip_address);
  }

  auto getPort() const noexcept -> const std::string& { return port_; }

  auto setPort(std::string port) noexcept -> void { port_ = std::move(port); }

  auto setSendTimeout(std::chrono::microseconds /*timeout*/) noexcept
      -> std::expected<std::monostate, std::error_code> {
    return std::monostate{};
  }

  auto sendAll(std::span<const uint8_t> data) noexcept
      -> std::expected<std::monostate, std::error_code> {
    sent_frames_.emplace_back(data.begin(), data.end());
    return std::monostate{};
  }

  auto recvSome(std::span<uint8_t> buffer) noexcept
      -> std::expected<std::size_t, std::error_code> {
    if (buffer.empty()) {
      return 0U;
    }
    std::unique_lock<std::mutex> lock(mtx_);
    cv_.wait(lock, [&] { return shutdown_ || !incoming_bytes_.empty(); });
    if (incoming_bytes_.empty()) {
      return std::unexpected{
          std::make_error_code(std::errc::operation_canceled)};
    }
    const auto count =
        std::min(buffer.size(), static_cast<std::size_t>(incoming_bytes_.size()));
    for (std::size_t i = 0; i < count; ++i) {
      buffer[i] = incoming_bytes_.front();
      incoming_bytes_.pop_front();
    }
    return count;
  }

  auto shutdown() noexcept
      -> std::expected<std::monostate, std::error_code> {
    {
      std::lock_guard<std::mutex> lock(mtx_);
      shutdown_ = true;
    }
    cv_.notify_all();
    return std::monostate{};
  }

  [[nodiscard]] auto isShutdown() const noexcept -> bool { return shutdown_; }

  auto connect(std::chrono::microseconds /*timeout*/) noexcept
      -> std::expected<std::monostate, std::error_code> {
    return std::monostate{};
  }

  void enqueueIncoming(const std::vector<uint8_t>& data) {
    {
      std::lock_guard<std::mutex> lock(mtx_);
      for (auto byte : data) {
        incoming_bytes_.push_back(byte);
      }
    }
    cv_.notify_all();
  }

  [[nodiscard]] auto sent_frames() const
      -> const std::vector<std::vector<uint8_t>>& {
    return sent_frames_;
  }

 private:
    std::string ip_address_;
    std::string port_;
    std::vector<std::vector<uint8_t>> sent_frames_;
    std::deque<uint8_t> incoming_bytes_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool shutdown_ = false;
};

class TestNode
    : public spw_rmap::internal::SpwRmapTCPNodeImpl<MockBackend> {
  using Base = spw_rmap::internal::SpwRmapTCPNodeImpl<MockBackend>;

 public:
  explicit TestNode(spw_rmap::SpwRmapTCPNodeConfig config)
      : Base(std::move(config)) {}

  void enqueueIncoming(const std::vector<uint8_t>& frame) {
    backend().enqueueIncoming(frame);
  }

  auto shutdown() noexcept
      -> std::expected<std::monostate, std::error_code> override {
    return backend().shutdown();
  }

  auto isShutdowned() noexcept -> bool override {
    return backend().isShutdown();
  }

 private:
  using Base::getBackend_;

  auto backend() -> MockBackend& { return *getBackend_(); }
};

auto makeFrame(std::span<const uint8_t> payload)
    -> std::vector<uint8_t> {
  std::vector<uint8_t> frame(12 + payload.size());
  frame[0] = 0x00;
  frame[1] = 0x00;
  frame[2] = 0x00;
  frame[3] = 0x00;
  const uint64_t length = payload.size();
  frame[4] = static_cast<uint8_t>((length >> 56) & 0xFF);
  frame[5] = static_cast<uint8_t>((length >> 48) & 0xFF);
  frame[6] = static_cast<uint8_t>((length >> 40) & 0xFF);
  frame[7] = static_cast<uint8_t>((length >> 32) & 0xFF);
  frame[8] = static_cast<uint8_t>((length >> 24) & 0xFF);
  frame[9] = static_cast<uint8_t>((length >> 16) & 0xFF);
  frame[10] = static_cast<uint8_t>((length >> 8) & 0xFF);
  frame[11] = static_cast<uint8_t>(length & 0xFF);
  std::copy(payload.begin(), payload.end(), frame.begin() + 12);
  return frame;
}

auto buildWriteReplyFrame(uint16_t transaction_id)
    -> std::vector<uint8_t> {
  spw_rmap::WriteReplyPacketBuilder builder;
  auto reply_addr = std::array<uint8_t, 1>{0x01};
  auto config = spw_rmap::WriteReplyPacketConfig{
      .replyAddress = reply_addr,
      .initiatorLogicalAddress = 0x34,
      .status = static_cast<uint8_t>(
          spw_rmap::PacketStatusCode::CommandExecutedSuccessfully),
      .targetLogicalAddress = 0xFE,
      .transactionID = transaction_id,
      .incrementMode = true,
      .verifyMode = true,
  };
  std::vector<uint8_t> payload(builder.getTotalSize(config));
  EXPECT_TRUE(builder.build(config, payload).has_value());
  return makeFrame(payload);
}

auto makeNodeConfig() -> spw_rmap::SpwRmapTCPNodeConfig {
  spw_rmap::SpwRmapTCPNodeConfig config;
  config.ip_address = "127.0.0.1";
  config.port = "10030";
  config.send_buffer_size = 512;
  config.recv_buffer_size = 512;
  return config;
}

auto makeTargetNode()
    -> std::shared_ptr<spw_rmap::TargetNodeBase> {
  std::vector<uint8_t> target_addr{0x20, 0x30};
  std::vector<uint8_t> reply_addr{0x10, 0x11};
  return std::make_shared<spw_rmap::TargetNodeDynamic>(
      0x34, std::move(target_addr), std::move(reply_addr));
}

TEST(SpwRmapTCPNodeImplTest, WriteAsyncCompletesAfterPoll) {
  TestNode node(makeNodeConfig());
  auto target_node = makeTargetNode();

  std::array<uint8_t, 4> payload{0xAA, 0xBB, 0xCC, 0xDD};
  std::atomic<bool> callback_called{false};

  auto future = node.writeAsync(
      target_node, 0x1000, payload,
      [&callback_called](const spw_rmap::Packet& packet) {
        callback_called = true;
        EXPECT_EQ(packet.type, spw_rmap::PacketType::WriteReply);
      });

  node.enqueueIncoming(buildWriteReplyFrame(0x0020));

  auto poll_result = node.poll();
  ASSERT_TRUE(poll_result.has_value());
  EXPECT_TRUE(poll_result.value());

  auto write_result = future.get();
  EXPECT_TRUE(write_result.has_value());
  EXPECT_TRUE(callback_called.load());
}

TEST(SpwRmapTCPNodeImplTest, WriteTimeoutReleasesTransactionId) {
  TestNode node(makeNodeConfig());
  auto target_node = makeTargetNode();
  std::array<uint8_t, 2> payload{0x01, 0x02};

  auto timeout_result = node.write(target_node, 0x2000, payload,
                                   std::chrono::milliseconds(1), 1);
  ASSERT_FALSE(timeout_result.has_value());
  EXPECT_EQ(timeout_result.error(), std::make_error_code(std::errc::timed_out));

  std::atomic<bool> callback_called{false};
  auto future = node.writeAsync(
      target_node, 0x2000, payload,
      [&callback_called](const spw_rmap::Packet&) { callback_called = true; });

  node.enqueueIncoming(buildWriteReplyFrame(0x0020));

  auto poll_result = node.poll();
  ASSERT_TRUE(poll_result.has_value());
  EXPECT_TRUE(poll_result.value());

  auto write_result = future.get();
  EXPECT_TRUE(write_result.has_value());
  EXPECT_TRUE(callback_called.load());
}

}  // namespace
