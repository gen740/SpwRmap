/**
 * @file LegacySpwRmap.cc
 * @brief Legacy SpaceWire RMAP library implementation.
 * @date 2025-03-01
 * @author gen740
 */
#include "SpwRmap/LegacySpwRmap.hh"

#include <RMAP.hh>
#include <SpaceWireIFOverTCP.hh>
#include <XMLUtilities/XMLLoader.hpp>
#include <XMLUtilities/XMLNode.hpp>
#include <chrono>
#include <memory>
#include <span>
#include <thread>

namespace SpwRmap {

auto join_span(const std::span<const uint8_t> numbers) -> std::string {
  std::ostringstream oss;
  if (numbers.empty()) {
    return "";
  }
  oss << static_cast<int>(numbers[0]);
  for (size_t i = 1; i < numbers.size(); ++i) {
    oss << " " << static_cast<int>(numbers[i]);
  }
  return oss.str();
}

class LegacySpwRmap::SpwPImpl {
 private:
  std::unique_ptr<SpaceWireIFOverTCP> spwif = nullptr;
  std::unique_ptr<RMAPEngine> rmap_engine = nullptr;
  std::unique_ptr<RMAPInitiator> rmap_initiator = nullptr;

  /**
   * @brief Target nodes
   * @key: Logical address
   * @value: Target node
   *
   * Logical address is a 8-bit integer larger than 32.
   * The valid logical address range is 32 to 255.
   */
  std::map<uint8_t, RMAPTargetNode *> target_nodes;

  void start_() {
    try {
      rmap_engine->start();
      rmap_initiator = std::make_unique<RMAPInitiator>(rmap_engine.get());
      rmap_initiator->setInitiatorLogicalAddress(0xFE);
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } catch (CxxUtilities::Exception &e) {
      throw std::runtime_error(
          std::format("Failed to start RMAP engine: {}", e.toString()));
    }
  };

 public:
  SpwPImpl(const SpwPImpl &) = delete;
  SpwPImpl(SpwPImpl &&) = delete;
  auto operator=(const SpwPImpl &) -> SpwPImpl & = delete;
  auto operator=(SpwPImpl &&) -> SpwPImpl & = delete;
  explicit SpwPImpl(std::string_view ip_address, uint32_t port) {
    try {
      spwif =
          std::make_unique<SpaceWireIFOverTCP>(std::string(ip_address), port);
      spwif->open();
      rmap_engine = std::make_unique<RMAPEngine>(spwif.get());
    } catch (CxxUtilities::Exception &e) {
      throw std::runtime_error(std::format(
          "Failed to initialize SpaceWire interface: {}", e.toString()));
    }
  };

  /**
   * @brief Destructor for SpwPImpl.
   *
   * Stops the RMAP engine if it is started.
   */
  ~SpwPImpl() {
    if (rmap_engine->isStarted()) {
      try {
        rmap_engine->stop();
      } catch (const std::exception &e) {
        std::println(stderr, "Failed to stop RMAP engine: {}", e.what());
        std::terminate();
      } catch (CxxUtilities::Exception &e) {
        std::println(stderr, "Failed to stop RMAP engine: {}", e.toString());
        std::terminate();
      }
    }
  };

  auto write(const TargetNodeBase &target_node, uint32_t memory_address,
             const std::span<const uint8_t> data) -> void {
    if (rmap_engine->isStarted() == false) {
      start_();
    }
    RMAPTargetNode rmap_target_node;
    rmap_target_node.setTargetLogicalAddress(
        target_node.getTargetLogicalAddress());
    rmap_target_node.setTargetLogicalAddress(
        target_node.getInitiatorLogicalAddress());
    rmap_target_node.setTargetSpaceWireAddress(
        {target_node.getTargetSpaceWireAddress().begin(),
         target_node.getTargetSpaceWireAddress().end()});
    rmap_target_node.setReplyAddress({target_node.getReplyAddress().begin(),
                                      target_node.getReplyAddress().end()});
    try {
      rmap_initiator->write(&rmap_target_node, memory_address,
                            const_cast<uint8_t *>(data.data()),  // NOLINT
                            data.size());
    } catch (RMAPInitiatorException &e) {
      throw std::runtime_error(
          std::format("RMAPInitiatorException: {}", e.toString()));
    } catch (RMAPReplyException &e) {
      throw std::runtime_error(
          std::format("RMAPReplyException: {}", e.toString()));
    }
  }

  auto read(const TargetNodeBase &target_node, uint32_t memory_address,
            const std::span<uint8_t> buffer) -> void {
    if (rmap_engine->isStarted() == false) {
      start_();
    }
    RMAPTargetNode rmap_target_node;
    rmap_target_node.setTargetLogicalAddress(
        target_node.getTargetLogicalAddress());
    rmap_target_node.setTargetLogicalAddress(
        target_node.getInitiatorLogicalAddress());
    rmap_target_node.setTargetSpaceWireAddress(
        {target_node.getTargetSpaceWireAddress().begin(),
         target_node.getTargetSpaceWireAddress().end()});
    rmap_target_node.setReplyAddress({target_node.getReplyAddress().begin(),
                                      target_node.getReplyAddress().end()});
    try {
      rmap_initiator->read(&rmap_target_node, memory_address, buffer.size(),
                           buffer.data());
    } catch (RMAPInitiatorException &e) {
      throw std::runtime_error(
          std::format("RMAPInitiatorException: {}", e.toString()));
    } catch (RMAPReplyException &e) {
      throw std::runtime_error(
          std::format("RMAPReplyException: {}", e.toString()));
    }
  }

  auto emitTimeCode(uint8_t timecode) -> void { spwif->emitTimecode(timecode); }
};

LegacySpwRmap::LegacySpwRmap(std::string_view ip_address, uint32_t port)
    : impl_(new SpwPImpl(ip_address, port)) {}
LegacySpwRmap::~LegacySpwRmap() = default;

auto LegacySpwRmap::write(const TargetNodeBase &target_node,
                          uint32_t memory_address,
                          const std::span<const uint8_t> data)
    -> std::expected<std::monostate, std::error_code> {
  try {
    impl_->write(target_node, memory_address, data);
  } catch (const std::invalid_argument &e) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  } catch (const std::runtime_error &e) {
    return std::unexpected(std::make_error_code(std::errc::io_error));
  }
  return std::monostate{};
}

auto LegacySpwRmap::read(const TargetNodeBase &target_node,
                         uint32_t memory_address, const std::span<uint8_t> data)
    -> std::expected<std::monostate, std::error_code> {
  try {
    impl_->read(target_node, memory_address, data);
  } catch (const std::invalid_argument &e) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  } catch (const std::runtime_error &e) {
    return std::unexpected(std::make_error_code(std::errc::io_error));
  }
  return std::monostate{};
}

auto LegacySpwRmap::emitTimeCode(uint8_t timecode)
    -> std::expected<std::monostate, std::error_code> {
  try {
    impl_->emitTimeCode(timecode);
  } catch (const std::runtime_error &e) {
    return std::unexpected(std::make_error_code(std::errc::io_error));
  }
  return std::monostate{};
}

};  // namespace SpwRmap
