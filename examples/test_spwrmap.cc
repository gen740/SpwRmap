#include <SpwRmap/SpwRmap.hh>
#include <SpwRmap/testing/SpwServer.hh>
#include <print>
#include <thread>

auto main() -> int {
  // SpwRmap::testing::SSDTP2Server server("0.0.0.0", "10032");
  // auto t = std::thread([&server]() {
  //   {
  //     std::println("Starting server on 0.0.0.0:10031");
  //     auto res = server.listen_once();
  //     if (!res.has_value()) {
  //       std::println("Error: {}", res.error().message());
  //       return;
  //     }
  //   }
  //   {
  //     auto res = server.run();
  //     if (!res.has_value()) {
  //       std::println("Error: {}", res.error().message());
  //       return;
  //     }
  //   }
  // });
  //
  // SpwRmap::SpwRmap rmap("localhost", 10032);
  //
  // rmap.initialize(1024, 1024);
  //
  // SpwRmap::TargetNode target_node{
  //     .logical_address = 0xFE,
  //     .target_spacewire_address = {0x00, 0x01, 0x02, 0x03},
  //     .reply_address = {}};
  //
  // rmap.addTargetNode(target_node);
  //
  // std::vector<uint8_t> data_to_write(64);
  // for (size_t i = 0; i < data_to_write.size(); ++i) {
  //   data_to_write[i] = static_cast<uint8_t>(i);
  // }
  //
  // auto res = rmap.write(0xFE, 0x00000000, data_to_write);
  // if (!res.has_value()) {
  //   std::println("Write error: {}", res.error().message());
  // } else {
  //   std::println("Write successful");
  // }
  //
  // std::vector<uint8_t> read_data(64);
  // res = rmap.read(0xFE, 0x00000000, read_data);
  // if (!res.has_value()) {
  //   std::println("Read error: {}", res.error().message());
  // } else {
  //   std::println("Read successful, data:");
  //   for (const auto &byte : read_data) {
  //     std::print("{:02X} ", byte);
  //   }
  //   std::println();
  // }
  //
  // server.stop();
  // if (t.joinable()) {
  //   t.join();
  // } else {
  //   std::println("Thread not joinable");
  // }
}
