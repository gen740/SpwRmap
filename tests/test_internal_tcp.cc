#include <gtest/gtest.h>

#include <SpwRmap/internal/TCPClient.hh>
#include <SpwRmap/internal/TCPServer.hh>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <future>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <thread>
#include <vector>

using SpwRmap::internal::TCPClient;
using SpwRmap::internal::TCPServer;

using namespace std::chrono_literals;

// Helper: bind to 127.0.0.1:0 to get an available port, then close.
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
  r = ::bind(fd, reinterpret_cast<sockaddr*>(&sin), sizeof(sin));
  if (r != 0) {
    const int e = errno;
    (void)::close(fd);
    throw std::system_error(e, std::system_category(), "bind");
  }
  socklen_t sl = sizeof(sin);
  r = ::getsockname(fd, reinterpret_cast<sockaddr*>(&sin), &sl);
  if (r != 0) {
    const int e = errno;
    (void)::close(fd);
    throw std::system_error(e, std::system_category(), "getsockname");
  }
  const uint16_t port = ntohs(sin.sin_port);
  do {
    r = ::close(fd);
  } while (r < 0 && errno == EINTR);
  return port;
}

std::mt19937 rng(std::random_device{}());

TEST(TcpClientServer, ServerRecieve) {
  size_t TEST_BUFFER_SIZE = 1024UL;
  const uint16_t port = pick_free_port();

  std::atomic<bool> server_stop{false};
  std::vector<uint8_t> server_recv_buf;

  server_recv_buf.resize(TEST_BUFFER_SIZE);
  bool server_emit_error = false;

  std::thread th([&]() {
    try {
      TCPServer server("127.0.0.1", port, 500ms, 500ms);
      auto total_recvd = 0U;

      std::vector<uint8_t> buf;
      buf.resize(16);
      while (!server_stop.load(std::memory_order_acquire)) {
        auto n = server.recv_some(buf);
        std::ranges::copy(std::span(buf).subspan(0, n), server_recv_buf.begin() + total_recvd);
        total_recvd += n;
        if (total_recvd == TEST_BUFFER_SIZE) {
          break;
        }
      }
    } catch (const std::system_error& e) {
      std::println("Server thread error: {}", e.what());
      server_emit_error = true;
    }
  });

  TCPClient client("localhost", port, 500ms, 500ms, 500ms);

  std::vector<uint8_t> msg;
  msg.resize(TEST_BUFFER_SIZE);

  for (auto& byte : msg) {
    byte = static_cast<uint8_t>(std::uniform_int_distribution<>(0, 255)(rng));
  }

  size_t mes_size_sent = 0;
  while (mes_size_sent < msg.size()) {
    size_t mes_size = std::uniform_int_distribution<>(1, 32)(rng);
    if (mes_size_sent + mes_size > msg.size()) {
      mes_size = msg.size() - mes_size_sent;
    }
    client.send_all(std::span<const uint8_t>(msg.data() + mes_size_sent, mes_size));
    mes_size_sent += mes_size;
  }
  std::this_thread::sleep_for(100ms);  // Give server time to process.

  server_stop.store(true, std::memory_order_release);
  EXPECT_EQ(server_recv_buf, msg);

  if (th.joinable()) {
    th.join();
  }
  EXPECT_FALSE(server_emit_error) << "Server thread emitted an error during execution.";
}

TEST(TcpClientServer, ClientRecieve) {
  size_t TEST_BUFFER_SIZE = 1024UL;
  const uint16_t port = pick_free_port();

  std::atomic<bool> server_stop{false};

  std::vector<uint8_t> msg;
  msg.resize(TEST_BUFFER_SIZE);
  for (auto& byte : msg) {
    byte = static_cast<uint8_t>(std::uniform_int_distribution<>(0, 255)(rng));
  }
  bool server_emit_error = false;
  std::thread th([&]() {
    try {
      TCPServer server("127.0.0.1", port, 500ms, 500ms);
      size_t mes_size_sent = 0;
      while (mes_size_sent < msg.size()) {
        size_t mes_size = std::uniform_int_distribution<>(1, 32)(rng);
        if (mes_size_sent + mes_size > msg.size()) {
          mes_size = msg.size() - mes_size_sent;
        }
        server.send_all(std::span<const uint8_t>(msg.data() + mes_size_sent, mes_size));
        mes_size_sent += mes_size;
      }
    } catch (const std::system_error& e) {
      std::println("Server thread error: {}", e.what());
      server_emit_error = true;
    }
  });

  TCPClient client("localhost", port, 500ms, 500ms, 500ms);
  std::vector<uint8_t> client_recv_buf;
  client_recv_buf.resize(TEST_BUFFER_SIZE);

  std::vector<uint8_t> buf;
  buf.resize(16);
  auto total_recvd = 0U;
  while (true) {
    auto n = client.recv_some(buf);
    std::ranges::copy(std::span(buf).subspan(0, n), client_recv_buf.begin() + total_recvd);
    total_recvd += n;
    if (total_recvd == TEST_BUFFER_SIZE) {
      break;
    }
  }
  server_stop.store(true, std::memory_order_release);
  EXPECT_EQ(msg, client_recv_buf);
  if (th.joinable()) {
    th.join();
  }
  EXPECT_FALSE(server_emit_error) << "Server thread emitted an error during execution.";
}
