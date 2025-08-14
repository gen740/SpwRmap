#include <SpwRmap/LegacySpwRmap.hh>
#include <array>
#include <chrono>
#include <print>
#include <span>
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
  SpwRmap::LegacySpwRmap spw_rmap("192.168.2.100", 10030);

  SpwRmap::TargetNode target_node;
  target_node.logical_address = 0xF2;

  target_node.target_spacewire_address = {2};
  target_node.reply_address = {3};

  spw_rmap.addTargetNode(target_node);

  std::array<uint8_t, 8> buffer{};
  std::array<uint8_t, 4> time_code_buffer{};
  spw_rmap.read(0xF2, 0x44a40000, buffer);

  std::print("{}", buffer);

  for (int i = 0; i < 30000; i++) {
    spw_rmap.read(0xF2, 0x44a40000, buffer);
    std::println("{}\tdata = {}", get_spw_ti(buffer), buffer);
    spw_rmap.read(0xF2, 0x44a40008, time_code_buffer);
    std::println("{}", time_code_buffer);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000 / 64));
  }

  return 0;
}
