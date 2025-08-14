

#include <SpwRmap/internal/TCPClient.hh>
#include <chrono>
#include <print>
#include <vector>

using namespace std::chrono_literals;

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

int main() {


  // auto client = SpwRmap::internal::TCPClient("localhost", 10032, 100ms, 100ms);
  // std::vector<uint8_t> buffer;
  // buffer.resize(1024);
  //
  // buffer[0] = 0x01;  // Example data to send
  // buffer[1] = 0x02;
  // buffer[2] = 0x03;
  // buffer[3] = 0x04;
  //
  // try {
  //   client.send_all(std::span<const uint8_t>(buffer.data(), 4));
  // } catch (const std::system_error& e) {
  //   std::println("Error receiving data: {}", e.what());
  // }
  //
  // for (const auto& byte : buffer) {
  //   std::print("{:02x} ", byte);
  // }
}
