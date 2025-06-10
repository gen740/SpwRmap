#include <SpwRmap/SpwRmap.hh>
#include <chrono>
#include <print>
#include <thread>

uint64_t get_spw_ti(const auto &vec) {
  if (vec.size() != 8) {
    throw std::invalid_argument("Vector size must be 8");
  }
  return (static_cast<uint64_t>(vec[3])) |
         (static_cast<uint64_t>(vec[2]) << 8) |
         (static_cast<uint64_t>(vec[1]) << 16) |
         (static_cast<uint64_t>(vec[0]) << 24) |
         (static_cast<uint64_t>(vec[7]) << 32) |
         (static_cast<uint64_t>(vec[6]) << 40) |
         (static_cast<uint64_t>(vec[5]) << 48) |
         (static_cast<uint64_t>(vec[4]) << 56);
}

auto main() -> int {
  SpwRmap::SpwRmap spw_rmap("192.168.2.100", 10030);

  SpwRmap::TargetNode target_node;
  target_node.logical_address = 0xF2;

  target_node.target_spacewire_address = {2};
  target_node.reply_address = {3};

  spw_rmap.addTargetNode(target_node);

  std::print("{}", spw_rmap.read(0xF2, 0x44a40000, 8));

  for (int i = 0; i < 30000; i++) {
    /*spw_rmap.emitTimeCode(static_cast<uint8_t>(i % 64));*/
    auto data = spw_rmap.read(0xF2, 0x44a40000, 8);
    std::println("{}\tdata = {}", get_spw_ti(data), data);
    auto timecode_data = spw_rmap.read(0xF2, 0x44a40008, 4);
    std::println("{}", timecode_data);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000 / 64));
  }

  return 0;
}
