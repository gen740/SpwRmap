#include <SpwRmap/LegacySpwRmapTCPNode.hh>
#include <array>
#include <chrono>
#include <print>
#include <span>
#include <thread>

auto get_spw_ti(const auto &vec) -> uint64_t {
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

auto main() -> int try {
  SpwRmap::LegacySpwRmapTCPNode spw_rmap("192.168.2.100", 10030);

  SpwRmap::TargetNodeFixed<1, 1> target_node{0xF2, {2}, {3}};

  std::array<uint8_t, 8> buffer{};
  std::array<uint8_t, 4> time_code_buffer{};
  {
    auto res = spw_rmap.read(target_node, 0x44a40000, buffer);
    if (!res.has_value()) {
      std::print("Failed to read data: {}\n", res.error().message());
      return 1;
    }
  }

  std::print("{}", buffer);

  for (int i = 0; i < 30000; i++) {
    {
      auto res = spw_rmap.read(target_node, 0x44a40000, buffer);
      if (!res.has_value()) {
        std::print("Failed to read data: {}\n", res.error().message());
        return 1;
      }
    }
    std::println("{}\tdata = {}", get_spw_ti(buffer), buffer);
    {
      auto res = spw_rmap.read(target_node, 0x44a40008, time_code_buffer);
      if (!res.has_value()) {
        std::print("Failed to read time code: {}\n", res.error().message());
        return 1;
      }
    }
    std::println("{}", time_code_buffer);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000 / 64));
  }

  return 0;
} catch (const std::exception &e) {
  std::fputs(e.what(), stderr);
  return 1;
} catch (...) {
  std::fputs("Unknown error occurred", stderr);
  return 1;
}
