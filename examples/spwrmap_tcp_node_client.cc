#include <chrono>
#include <iostream>
#include <memory>
#include <spw_rmap/spw_rmap_tcp_node.hh>
#include <tuple>

using namespace std::chrono_literals;

auto main() -> int {
  auto config = spw_rmap::SpwRmapTCPNodeConfig{.ip_address = "192.168.1.2",
                                               .port = "10032"};

  spw_rmap::SpwRmapTCPNode client(config);
  std::ignore = client.connect(0ms, 0ms, 0ms);

  auto t = std::thread([&client] -> void { client.runLoop(); });

  std::this_thread::sleep_for(500ms);

  auto target = std::make_shared<spw_rmap::TargetNodeDynamic>(
      0x34, std::vector<uint8_t>{0x01}, std::vector<uint8_t>{});

  // auto res = client
  //                .readAsync(target, 0x00000000, 16,
  //                           [](spw_rmap::Packet packet) noexcept -> void {
  //                             std::cout
  //                                 << "Received Read Response, Transaction ID:
  //                                 "
  //                                 << packet.transactionID
  //                                 << ", Data Length: " << packet.data.size()
  //                                 << "\n";
  //                           })
  //                .get();

  auto data = std::vector<uint8_t>(16);
  auto res = client.read(target, 0x00000000, data);

  if (!res.has_value()) {
    std::cerr << "ReadAsync failed: " << res.error().message() << "\n";
  }

  t.join();

  return 0;
}
