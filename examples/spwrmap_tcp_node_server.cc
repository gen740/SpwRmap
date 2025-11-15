#include <spw_rmap/internal/spw_rmap_tcp_node_server.hh>

auto main() -> int {
  auto config = spw_rmap::internal::SpwRmapTCPNodeConfig{
      .ip_address = "0.0.0.0", .port = "10032"};

  spw_rmap::internal::SpwRmapTCPNodeServer server(config);

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

  server.runLoop();
  return 0;
}
