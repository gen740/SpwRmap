#include <spw_rmap/spw_rmap_tcp_node_server.hh>

using namespace std::chrono_literals;

auto main() -> int {
  auto config = spw_rmap::internal::SpwRmapTCPNodeConfig{
      .ip_address = "0.0.0.0", .port = "10032"};

  spw_rmap::internal::SpwRmapTCPNodeServer server(config);
  server.acceptOnce(0ms, 0ms);

  server.registerOnRead([](spw_rmap::Packet packet) noexcept -> void {
    std::cout << "Received Read Packet, Transaction ID: "
              << packet.transactionID << ", Data Length: " << packet.data.size()
              << "\n";
  });

  server.registerOnWrite([](spw_rmap::Packet packet) noexcept -> void {
    std::cout << "Received Write Packet, Transaction ID: "
              << packet.transactionID << ", Data Length: " << packet.data.size()
              << "\n";
  });

  auto t = std::thread([&server] -> void { server.runLoop(); });

  t.join();

  return 0;
}
