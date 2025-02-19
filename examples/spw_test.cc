#include <iostream>

#include <SpwRmap.hh>
#include <thread>


auto main() -> int {
  SpwRmap::SpwRmap spw_rmap("192.168.1.10", 100);
  SpwRmap::TargetNode target_node;
  target_node.logical_address = 0x20;
  target_node.target_spacewire_address = {0x01, 0x02, 0x03};
  target_node.reply_address = {0x04, 0x05, 0x06};

  spw_rmap.addTargetNode(target_node);

  std::cout << "Hello, World!" << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(10));
  return 0;
}
