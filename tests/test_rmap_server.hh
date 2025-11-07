#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/fcntl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <print>
#include <span>
#include <system_error>
#include <thread>

#include "SpwRmap/SpwRmap.hh"
#include "SpwRmap/testing/SpwServer.hh"

static auto pick_free_port() -> uint16_t {
  const int fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (fd < 0) {
    throw std::system_error(errno, std::system_category(), "socket");
  }
  int r = 0;
  sockaddr_in sin{};
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  sin.sin_port = htons(0);
  r = ::bind(fd, reinterpret_cast<sockaddr*>(&sin), sizeof(sin));  // NOLINT
  if (r != 0) {
    const int e = errno;
    (void)::close(fd);
    throw std::system_error(e, std::system_category(), "bind");
  }
  socklen_t sl = sizeof(sin);
  r = ::getsockname(fd, reinterpret_cast<sockaddr*>(&sin), &sl);  // NOLINT
  if (r != 0) {
    const int e = errno;
    (void)::close(fd);
    throw std::system_error(e, std::system_category(), "getsockname");
  }
  const uint16_t port = ntohs(sin.sin_port);
  do {  // NOLINT
    r = ::close(fd);
  } while (r < 0 && errno == EINTR);
  return port;
}

TEST(SpwRmap, WithSimpleTCPServer) {
  const uint16_t port = pick_free_port();
  const std::string port_str = std::to_string(port);

  std::thread server_thread([&]() -> void {
    SpwRmap::testing::SSDTP2Server server("0.0.0.0", port_str);
    auto res = server.run();
    if (!res.has_value()) {
      if (res.error() == std::make_error_code(std::errc::connection_reset)) {
        return;
      } else {
        FAIL() << "Server error: " << res.error().message();
      }
    }
  });

  SpwRmap::SpwRmap rmap("localhost", port);

  std::array<uint8_t, 1024> send_buffer{};
  std::array<uint8_t, 1024> recv_buffer{};
  rmap.initialize(send_buffer, recv_buffer);

  if (server_thread.joinable()) {
    server_thread.join();
  }
}
