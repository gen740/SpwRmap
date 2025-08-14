#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <random>
#include <thread>

#include "RMAPEngine.hh"
#include "RMAPInitiator.hh"
#include "SpaceWireIF.hh"
#include "SpwRmap/PacketBuilder.hh"
#include "SpwRmap/PacketParser.hh"

class SpaceWireIFDummy final : public SpaceWireIF {
 private:
  std::atomic_bool sent_{false};
  mutable std::mutex receive_mutex_;
  std::condition_variable cond_;

  std::mutex packet_mutex_;
  std::vector<uint8_t> packet_data_;

  uint16_t transaction_id_{0};

  SpwRmap::ReadReplyPacketBuilder read_reply_packet_builder_;
  SpwRmap::WriteReplyPacketBuilder write_reply_packet_builder_;

 public:
  void open() override {}
  void receive(std::vector<uint8_t>* buffer) override {
    std::unique_lock<std::mutex> lock(packet_mutex_);
    cond_.wait_for(lock, std::chrono::milliseconds(100),
                   [this] { return sent_.load(); });
    if (!sent_.load()) {
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::vector<uint8_t> replyAddress = {};
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
    auto res = read_reply_packet_builder_.build({
        .replyAddress = replyAddress,
        .initiatorLogicalAddress = 0x35,
        .status = 0x00,
        .targetLogicalAddress = 0xEF,
        .transactionID = transaction_id_,
        .data = data,
        .incrementMode = true,
    });
    if (!res.has_value()) {
      FAIL() << "Failed to build read reply packet: " << res.error().message();
    }
    std::ranges::copy(*read_reply_packet_builder_.getPacket(),
                      std::back_inserter(*buffer));

    sent_.store(false);
  }
  void send(uint8_t* data, size_t length,
            SpaceWireEOPMarker::EOPType) override {
    std::lock_guard<std::mutex> lock(packet_mutex_);
    packet_data_.clear();
    packet_data_.insert(packet_data_.end(), data, data + length);
    sent_.store(true);
    cond_.notify_all();
  }
  void setTxLinkRate(uint32_t) override {};
  auto getTxLinkRateType() -> uint32_t override { return 10; }
  void emitTimecode(uint8_t, uint8_t) override {}
  void setTimeoutDuration(double) override {}
  void cancelReceive() override {}
  auto getPacketData() -> std::vector<uint8_t> {
    std::lock_guard<std::mutex> lock(packet_mutex_);
    return packet_data_;
  }
  auto setTransactionID(uint16_t transaction_id) -> void {
    transaction_id_ = transaction_id;
  }
};

class LegacySpwRmapReadPacketBuilder {
 private:
  std::unique_ptr<RMAPEngine> rmap_engine_;
  std::unique_ptr<SpaceWireIFDummy> spwif_;
  std::unique_ptr<RMAPInitiator> rmap_initiator_;

  LegacySpwRmapReadPacketBuilder() noexcept {
    if (!rmap_engine_) {
      spwif_ = std::make_unique<SpaceWireIFDummy>();
      spwif_->open();
      rmap_engine_ = std::make_unique<RMAPEngine>(spwif_.get());
      rmap_initiator_ = std::make_unique<RMAPInitiator>(rmap_engine_.get());
      rmap_engine_->start();
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

 public:
  LegacySpwRmapReadPacketBuilder(const LegacySpwRmapReadPacketBuilder&) =
      delete;
  LegacySpwRmapReadPacketBuilder(LegacySpwRmapReadPacketBuilder&&) = delete;
  auto operator=(const LegacySpwRmapReadPacketBuilder&)
      -> LegacySpwRmapReadPacketBuilder& = delete;
  auto operator=(LegacySpwRmapReadPacketBuilder&&)
      -> LegacySpwRmapReadPacketBuilder& = delete;
  ~LegacySpwRmapReadPacketBuilder() = default;

  static auto build(SpwRmap::ReadPacketConfig config) {
    static std::once_flag flag;
    std::call_once(
        flag, [] { instance_ptr_ = new LegacySpwRmapReadPacketBuilder(); });
    RMAPTargetNode node;
    node.setInitiatorLogicalAddress(config.initiatorLogicalAddress);
    node.setTargetLogicalAddress(config.targetLogicalAddress);
    node.setDefaultKey(config.key);
    std::vector<uint8_t> targetSpaceWireAddress(
        config.targetSpaceWireAddress.begin(),
        config.targetSpaceWireAddress.end());
    node.setTargetSpaceWireAddress(targetSpaceWireAddress);
    std::vector<uint8_t> replyAddress(config.replyAddress.begin(),
                                      config.replyAddress.end());
    node.setReplyAddress(replyAddress);
    instance_ptr_->rmap_initiator_->setInitiatorLogicalAddress(
        config.initiatorLogicalAddress);
    instance_ptr_->rmap_initiator_->setTransactionID(config.transactionID);
    instance_ptr_->spwif_->setTransactionID(config.transactionID);
    instance_ptr_->rmap_initiator_->setIncrementMode(config.incrementMode);
    instance_ptr_->rmap_initiator_->setVerifyMode(false);
    instance_ptr_->rmap_initiator_->setReplyMode(true);
    std::vector<uint8_t> data(config.dataLength);
    instance_ptr_->rmap_initiator_->read(&node, config.address,
                                         config.dataLength, data.data());
    return instance_ptr_->spwif_->getPacketData();
  }

 private:
  inline static LegacySpwRmapReadPacketBuilder* instance_ptr_;
};

class LegacySpwRmapWritePacketBuilder {
 private:
  std::unique_ptr<RMAPEngine> rmap_engine_;
  std::unique_ptr<SpaceWireIFDummy> spwif_;
  std::unique_ptr<RMAPInitiator> rmap_initiator_;

  LegacySpwRmapWritePacketBuilder() noexcept {
    if (!rmap_engine_) {
      spwif_ = std::make_unique<SpaceWireIFDummy>();
      spwif_->open();
      rmap_engine_ = std::make_unique<RMAPEngine>(spwif_.get());
      rmap_initiator_ = std::make_unique<RMAPInitiator>(rmap_engine_.get());
      rmap_engine_->start();
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

 public:
  LegacySpwRmapWritePacketBuilder(const LegacySpwRmapWritePacketBuilder&) =
      delete;
  LegacySpwRmapWritePacketBuilder(LegacySpwRmapWritePacketBuilder&&) = delete;
  auto operator=(const LegacySpwRmapWritePacketBuilder&)
      -> LegacySpwRmapWritePacketBuilder& = delete;
  auto operator=(LegacySpwRmapWritePacketBuilder&&)
      -> LegacySpwRmapWritePacketBuilder& = delete;
  ~LegacySpwRmapWritePacketBuilder() = default;

  static auto build(SpwRmap::WritePacketConfig config) {
    static std::once_flag flag;
    std::call_once(
        flag, [] { instance_ptr_ = new LegacySpwRmapWritePacketBuilder(); });
    RMAPTargetNode node;
    node.setInitiatorLogicalAddress(config.initiatorLogicalAddress);
    node.setTargetLogicalAddress(config.targetLogicalAddress);
    node.setDefaultKey(config.key);
    std::vector<uint8_t> targetSpaceWireAddress(
        config.targetSpaceWireAddress.begin(),
        config.targetSpaceWireAddress.end());
    node.setTargetSpaceWireAddress(targetSpaceWireAddress);
    std::vector<uint8_t> replyAddress(config.replyAddress.begin(),
                                      config.replyAddress.end());
    node.setReplyAddress(replyAddress);
    instance_ptr_->rmap_initiator_->setInitiatorLogicalAddress(
        config.initiatorLogicalAddress);
    instance_ptr_->rmap_initiator_->setTransactionID(config.transactionID);
    instance_ptr_->spwif_->setTransactionID(config.transactionID);
    instance_ptr_->rmap_initiator_->setIncrementMode(config.incrementMode);
    instance_ptr_->rmap_initiator_->setVerifyMode(config.verifyMode);
    instance_ptr_->rmap_initiator_->setReplyMode(config.reply);

    instance_ptr_->rmap_initiator_->write(
        &node, config.address,
        const_cast<uint8_t*>(config.data.data()),  // NOLINT
        config.data.size());

    return instance_ptr_->spwif_->getPacketData();
  }

 private:
  inline static LegacySpwRmapWritePacketBuilder* instance_ptr_;
};

std::mt19937 random_engine(std::random_device{}());  // NOLINT

TEST(PacketBuilder, ReadPacketBuilder) {
  std::uniform_int_distribution<uint8_t> distribution(0, 255);
  std::uniform_int_distribution<uint8_t> distribution_lt(1, 31);
  std::uniform_int_distribution<uint8_t> distribution_gt(32, 255);
  std::uniform_int_distribution<uint16_t> distribution16(0, (1UL << 16) - 1);
  std::uniform_int_distribution<uint32_t> distribution32(0, (1UL << 32) - 1);
  std::uniform_int_distribution<uint8_t> node_num(0, 12);

  std::vector<uint8_t> targetSpaceWireAddress{};
  std::vector<uint8_t> replyAddress{};

  for (size_t num = 0; num < 25; num++) {
    targetSpaceWireAddress.clear();
    for (int i = 0; i < node_num(random_engine); i++) {
      targetSpaceWireAddress.push_back(distribution_lt(random_engine));
    }
    replyAddress.clear();
    for (int i = 0; i < node_num(random_engine); i++) {
      replyAddress.push_back(distribution_lt(random_engine));
    }
    SpwRmap::ReadPacketConfig config = {
        .targetSpaceWireAddress = targetSpaceWireAddress,
        .replyAddress = replyAddress,
        .targetLogicalAddress = distribution_gt(random_engine),
        .initiatorLogicalAddress = distribution_gt(random_engine),
        .transactionID = distribution16(random_engine),
        .extendedAddress = 0x00,
        .address = distribution32(random_engine),
        .dataLength = distribution32(random_engine) & 0x00FFFFFF,
        .key = distribution(random_engine),
        .incrementMode = distribution(random_engine) % 2 == 0,
    };

    auto legacy_packet = LegacySpwRmapReadPacketBuilder::build(config);

    auto read_packet_builder = SpwRmap::ReadPacketBuilder();
    {
      auto res = read_packet_builder.build(config);
      if (!res.has_value()) {
        FAIL() << "Failed to build read packet: " << res.error().message();
      }
    }

    std::array<uint8_t, 1024> packet_buffer{};
    auto read_packet_builder_fixed_buf = SpwRmap::ReadPacketBuilder();
    read_packet_builder_fixed_buf.setBuffer(packet_buffer);
    {
      auto res = read_packet_builder_fixed_buf.build(config);
      if (!res.has_value()) {
        FAIL() << "Failed to build read packet with fixed buffer: "
               << res.error().message();
      }
    }

    auto packet_array = *read_packet_builder.getPacket();
    auto packet_array_fixed = *read_packet_builder_fixed_buf.getPacket();

    ASSERT_TRUE(std::ranges::equal(legacy_packet, packet_array));
    ASSERT_TRUE(std::ranges::equal(legacy_packet, packet_array_fixed));

    SpwRmap::PacketParser parser;
    auto status = parser.parse(packet_array);
    ASSERT_EQ(status, SpwRmap::PacketParser::Status::Success);

    ASSERT_TRUE(std::ranges::equal(parser.getPacket().targetSpaceWireAddress,
                                   targetSpaceWireAddress));
    ASSERT_TRUE(
        std::ranges::equal(parser.getPacket().replyAddress, replyAddress));
    ASSERT_EQ(parser.getPacket().targetLogicalAddress,
              config.targetLogicalAddress);
    ASSERT_EQ(parser.getPacket().initiatorLogicalAddress,
              config.initiatorLogicalAddress);
    ASSERT_EQ(parser.getPacket().transactionID, config.transactionID);
    ASSERT_EQ(parser.getPacket().extendedAddress, config.extendedAddress);
    ASSERT_EQ(parser.getPacket().address, config.address);
    ASSERT_EQ(parser.getPacket().dataLength, config.dataLength);
    ASSERT_EQ(parser.getPacket().key, config.key);
  }
}

TEST(PacketBuilder, WritePacketBuilder) {
  std::uniform_int_distribution<uint8_t> distribution(0, 255);
  std::uniform_int_distribution<uint8_t> distribution_lt(1, 31);
  std::uniform_int_distribution<uint8_t> distribution_gt(32, 255);
  std::uniform_int_distribution<uint16_t> distribution16(0, (1UL << 16) - 1);
  std::uniform_int_distribution<uint32_t> distribution32(0, (1UL << 32) - 1);
  std::uniform_int_distribution<uint8_t> node_num(0, 12);

  std::vector<uint8_t> targetSpaceWireAddress{};
  std::vector<uint8_t> replyAddress{};

  for (int num = 0; num < 25; num++) {
    targetSpaceWireAddress.clear();
    for (int i = 0; i < node_num(random_engine); i++) {
      targetSpaceWireAddress.push_back(distribution_lt(random_engine));
    }
    replyAddress.clear();
    for (int i = 0; i < node_num(random_engine); i++) {
      replyAddress.push_back(distribution_lt(random_engine));
    }
    std::vector<uint8_t> data;
    for (int i = 0; i < distribution(random_engine); i++) {
      data.push_back(distribution(random_engine));
    }
    SpwRmap::WritePacketConfig config = {
        .targetSpaceWireAddress = targetSpaceWireAddress,
        .replyAddress = replyAddress,
        .targetLogicalAddress = distribution_gt(random_engine),
        .initiatorLogicalAddress = distribution_gt(random_engine),
        .transactionID = distribution16(random_engine),
        .key = distribution(random_engine),
        .extendedAddress = 0x00,
        .address = distribution32(random_engine),
        .incrementMode = distribution(random_engine) % 2 == 0,
        .reply = distribution(random_engine) % 2 == 0,
        .verifyMode = distribution(random_engine) % 2 == 0,
        .data = data,
    };
    auto legacy_packet = LegacySpwRmapWritePacketBuilder::build(config);
    auto read_packet_builder = SpwRmap::WritePacketBuilder();
    {
      auto res = read_packet_builder.build(config);
      if (!res.has_value()) {
        FAIL() << "Failed to build write packet: " << res.error().message();
      }
    }

    std::array<uint8_t, 1024> packet_buffer{};
    auto read_packet_builder_fixed_array = SpwRmap::WritePacketBuilder();
    read_packet_builder_fixed_array.setBuffer(packet_buffer);
    {
      auto res = read_packet_builder_fixed_array.build(config);
      if (!res.has_value()) {
        FAIL() << "Failed to build write packet with fixed buffer: "
               << res.error().message();
      }
    }

    auto packet_array = *read_packet_builder.getPacket();
    auto packet_array_fixed = *read_packet_builder_fixed_array.getPacket();
    ASSERT_TRUE(std::ranges::equal(legacy_packet, packet_array));
    ASSERT_TRUE(std::ranges::equal(legacy_packet, packet_array_fixed));

    {  // Test PacketParser
      SpwRmap::PacketParser parser;
      auto status = parser.parse(packet_array);
      ASSERT_EQ(status, SpwRmap::PacketParser::Status::Success);
      ASSERT_TRUE(std::ranges::equal(parser.getPacket().targetSpaceWireAddress,
                                     targetSpaceWireAddress));
      ASSERT_TRUE(
          std::ranges::equal(parser.getPacket().replyAddress, replyAddress));
      ASSERT_EQ(parser.getPacket().targetLogicalAddress,
                config.targetLogicalAddress);
      ASSERT_EQ(parser.getPacket().initiatorLogicalAddress,
                config.initiatorLogicalAddress);
      ASSERT_EQ(parser.getPacket().transactionID, config.transactionID);
      ASSERT_EQ(parser.getPacket().key, config.key);
      ASSERT_EQ(parser.getPacket().extendedAddress, config.extendedAddress);
      ASSERT_EQ(parser.getPacket().address, config.address);
      ASSERT_TRUE(std::ranges::equal(parser.getPacket().data, config.data));
    }
  }
}
