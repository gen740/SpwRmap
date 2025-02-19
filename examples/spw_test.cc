#include <SpwRmap.hh>
#include <print>

auto main() -> int {
  SpwRmap::SpwRmap spw_rmap("192.168.2.100", 10080);

  SpwRmap::TargetNode target_node;
  target_node.logical_address = 0xFE;
  target_node.target_spacewire_address = {};
  target_node.reply_address = {};

  std::print("{}", spw_rmap.read(0xFE, 0x0430, 4));
  std::print("{}", spw_rmap.read(0xFE, 0x0434, 1));

  spw_rmap.addTargetNode(target_node);

  return 0;
}
