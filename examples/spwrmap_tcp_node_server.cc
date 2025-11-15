#include <random>
#include <spw_rmap/spw_rmap_tcp_node_server.hh>

using namespace std::chrono_literals;

auto main() -> int {
  std::random_device rd;
  std::mt19937 gen(rd());

  auto config =
      spw_rmap::SpwRmapTCPNodeConfig{.ip_address = "0.0.0.0", .port = "10032"};

  spw_rmap::SpwRmapTCPNodeServer server(config);
  server.acceptOnce(0ms, 0ms);

  server.registerOnRead(
      [&gen](spw_rmap::Packet packet) noexcept -> std::vector<uint8_t> {
        std::vector<uint8_t> data(packet.dataLength);
        std::cout << "Generating random data of length: " << packet.dataLength
                  << "\n";
        for (unsigned char& i : data) {
          i = static_cast<uint8_t>(
              std::uniform_int_distribution<>(0, 255)(gen));
        }
        std::cout << "Received Read Packet, Transaction ID: "
                  << packet.transactionID
                  << ", Data Length: " << packet.data.size() << "\n";
        return data;
      });

  server.registerOnWrite([](spw_rmap::Packet packet) noexcept -> void {
    std::cout << "Received Write Packet, Transaction ID: "
              << packet.transactionID << ", Data Length: " << packet.data.size()
              << "\n";
  });

  server.poll();

  return 0;
}
