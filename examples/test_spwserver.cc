#include <SpwRmap/SpwRmap.hh>
#include <SpwRmap/testing/SpwServer.hh>
#include <print>

auto main() -> int {
  SpwRmap::testing::SSDTP2Server server("0.0.0.0", "10032");
  {
    std::println("Starting server on 0.0.0.0:10031");
  }
  {
    std::println("Running server");
    auto res = server.run();
    std::println("Server stopped");
    if (!res.has_value()) {
      std::println("Error: {}", res.error().message());
      return 1;
    }
  }
  server.stop();
}
