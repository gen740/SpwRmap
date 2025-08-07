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
    rmap_engine->start();
    rmap_initiator = std::make_unique<RMAPInitiator>(rmap_engine.get());
    rmap_initiator->setInitiatorLogicalAddress(0xFE);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  };

 public:
  explicit SpwPImpl(std::string_view ip_address, uint32_t port) {
    spwif = std::make_unique<SpaceWireIFOverTCP>(std::string(ip_address), port);
    spwif->open();
    rmap_engine = std::make_unique<RMAPEngine>(spwif.get());
  };

  ~SpwPImpl() {
    if (rmap_engine->isStarted()) {
      rmap_engine->stop();
    }
  };

  /**
   * @brief Adds a target node to the list.
   *
   * The SpaceWireRMAPLibrary's target node has to be initialized uisng XMLNode.
   * This function wraps the initialization process.
   *
   * @param target_node The target node to add.
   */
  auto addTargetNode(const TargetNode &target_node) -> void {
    if (target_node.logical_address < 32) [[unlikely]] {
      throw std::invalid_argument("Logical address must be larger than 32.");
    }

    std::unique_ptr<XMLNode> node;

    XMLLoader loader;
    XMLNode *topnode;

    join_span(std::span{target_node.target_spacewire_address});

    auto xml_string =
        std::format(R"(
      <RMAPTargetNode id="Null">
        <TargetLogicalAddress>{}</TargetLogicalAddress>
        <TargetSpaceWireAddress>{}</TargetSpaceWireAddress>
        <ReplyAddress>{}</ReplyAddress>
        <Key>{}</Key>
      </RMAPTargetNode>)",
                    target_node.logical_address, join_span(target_node.target_spacewire_address),
                    join_span(target_node.reply_address), 2);

    loader.loadFromString(&topnode, xml_string);
    auto rmap_target_node = RMAPTargetNode::constructFromXMLNode(topnode);
    target_nodes.insert(std::make_pair(target_node.logical_address, rmap_target_node));
  }

  auto write(uint8_t logical_address, uint32_t memory_address, const std::span<const uint8_t> data)
      -> void {
    if (rmap_engine->isStarted() == false) {
      start_();
    }
    if (target_nodes.find(logical_address) == target_nodes.end()) {
      throw std::invalid_argument("Target node not found.");
    }
    RMAPTargetNode *target_node = target_nodes[logical_address];
    try {
      rmap_initiator->write(target_node, memory_address, (uint8_t *)(data.data()), data.size());
    } catch (RMAPInitiatorException &e) {
      throw std::runtime_error(std::format("RMAPInitiatorException: {}", e.toString()));
    } catch (RMAPReplyException &e) {
      throw std::runtime_error(std::format("RMAPReplyException: {}", e.toString()));
    }
  }

  auto read(uint8_t logical_address, uint32_t memory_address, const std::span<uint8_t> buffer)
      -> void {
    if (rmap_engine->isStarted() == false) {
      start_();
    }
    if (target_nodes.find(logical_address) == target_nodes.end()) {
      throw std::invalid_argument("Target node not found.");
    }
    auto target_node_ptr = target_nodes.at(logical_address);
    try {
      rmap_initiator->read(target_node_ptr, memory_address, buffer.size(), buffer.data());
    } catch (RMAPInitiatorException &e) {
      throw std::runtime_error(std::format("RMAPInitiatorException: {}", e.toString()));
    } catch (RMAPReplyException &e) {
      throw std::runtime_error(std::format("RMAPReplyException: {}", e.toString()));
    }
  }

  auto emitTimeCode(uint8_t timecode) -> void { spwif->emitTimecode(timecode); }
};

LegacySpwRmap::LegacySpwRmap(std::string_view ip_address, uint32_t port)
    : impl_(new SpwPImpl(ip_address, port)) {}
LegacySpwRmap::~LegacySpwRmap() = default;

auto LegacySpwRmap::addTargetNode(const TargetNode &target_node) -> void {
  impl_->addTargetNode(target_node);
}
auto LegacySpwRmap::write(uint8_t logical_address, uint32_t memory_address,
                          const std::span<const uint8_t> data) -> void {
  impl_->write(logical_address, memory_address, data);
}

auto LegacySpwRmap::read(uint8_t logical_address, uint32_t memory_address,
                         const std::span<uint8_t> data) -> void {
  impl_->read(logical_address, memory_address, data);
}
auto LegacySpwRmap::emitTimeCode(uint8_t timecode) -> void { impl_->emitTimeCode(timecode); }

};  // namespace SpwRmap
