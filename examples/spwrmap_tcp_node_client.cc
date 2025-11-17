#include <chrono>
#include <iostream>
#include <memory>
#include <spw_rmap/spw_rmap_tcp_node.hh>
#include <tuple>

using namespace std::chrono_literals;

auto main() -> int {
  auto config = spw_rmap::SpwRmapTCPNodeConfig{.ip_address = "localhost",
                                               .port = "10032"};

  spw_rmap::SpwRmapTCPNode client(config);
  std::ignore = client.connect(200ms);

  auto t = std::thread([&client] -> void {
    auto res = client.runLoop();
    if (!res.has_value()) {
      std::cerr << "runLoop error: " << res.error().message() << "\n";
    }
  });

  std::this_thread::sleep_for(500ms);

  auto target = std::make_shared<spw_rmap::TargetNodeDynamic>(
      0x34, std::vector<uint8_t>{0x01}, std::vector<uint8_t>{0x02});

  auto data = std::vector<uint8_t>(16);
  auto res = client.read(target, 0x00000000, data);

  if (!res.has_value()) {
    std::cerr << "ReadAsync failed: " << res.error().message() << "\n";
  }

  t.join();

  return 0;
}
