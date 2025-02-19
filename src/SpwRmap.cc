#include "SpwRmap.hh"
#include <RMAP.hh>
#include <SpaceWireIFOverTCP.hh>

#include <XMLUtilities/XMLLoader.hpp>
#include <XMLUtilities/XMLNode.hpp>
#include <span>

namespace SpwRmap {

std::string join_span(const std::span<const uint8_t> numbers) {
  std::ostringstream oss;
  if (!numbers.empty()) {
    oss << static_cast<int>(numbers[0]); // 最初の要素
    for (size_t i = 1; i < numbers.size(); ++i) {
      oss << " " << static_cast<int>(numbers[i]); // 空白区切りで追加
    }
  }
  return oss.str();
}

class SpwRmap::SpwPImpl {

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
  std::map<uint8_t, std::unique_ptr<RMAPTargetNode>> target_nodes;

public:
  SpwPImpl(std::string_view ip_address, uint32_t port) {
    spwif = std::make_unique<SpaceWireIFOverTCP>(std::string(ip_address), port);
    rmap_engine = std::make_unique<RMAPEngine>(spwif.get());
  };

  ~SpwPImpl() {
    if (rmap_engine->isStarted()) {
      rmap_engine->stop();
    }
  };

  void start() {
    rmap_engine->start();
    rmap_initiator = std::make_unique<RMAPInitiator>(rmap_engine.get());
  };

  void addTargetNode(const TargetNode &target_node) {
    if (target_node.logical_address < 32) [[unlikely]] {
      throw std::invalid_argument("Logical address must be larger than 32.");
    } else if (target_node.logical_address > 255) [[unlikely]] {
      throw std::invalid_argument("Logical address must be less than 255.");
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
                    target_node.logical_address,
                    join_span(target_node.target_spacewire_address),
                    join_span(target_node.reply_address), 0);

    std::println("{}", xml_string);
    loader.loadFromString(&topnode, xml_string);
    std::println("{}", (void *)topnode);
    auto rmap_target_node = RMAPTargetNode::constructFromXMLNode(topnode);
    target_nodes[target_node.logical_address] =
        std::unique_ptr<RMAPTargetNode>(rmap_target_node);
  }

  void write(uint8_t logical_address, uint32_t memory_address,
             const std::span<uint8_t> data) {
    if (target_nodes.find(logical_address) == target_nodes.end()) {
      throw std::invalid_argument("Target node not found.");
    }
    RMAPTargetNode *target_node = target_nodes[logical_address].get();
    rmap_initiator->write(target_node, memory_address, data.data(),
                          data.size());
  }

  auto read(uint8_t logical_address, uint32_t memory_address, uint32_t length)
      -> std::vector<uint8_t> {
    if (target_nodes.find(logical_address) == target_nodes.end()) {
      throw std::invalid_argument("Target node not found.");
    }
    RMAPTargetNode *target_node = target_nodes[logical_address].get();
    std::vector<uint8_t> buffer(length);
    rmap_initiator->read(target_node, memory_address, length, buffer.data());
    return buffer;
  }
};

SpwRmap::SpwRmap(std::string_view ip_address, uint32_t port)
    : pImpl(std::make_shared<SpwPImpl>(ip_address, port)) {}

void SpwRmap::addTargetNode(const TargetNode &target_node) {
  pImpl->addTargetNode(target_node);
}

void SpwRmap::write(uint8_t logical_address, uint32_t memory_address,
                    const std::span<uint8_t> data) {
  pImpl->write(logical_address, memory_address, data);
}

auto SpwRmap::read(uint8_t logical_address, uint32_t memory_address,
                   uint32_t length) -> std::vector<uint8_t> {
  return pImpl->read(logical_address, memory_address, length);
}

}; // namespace SpwRmap
