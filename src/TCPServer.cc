#include "SpwRmap/internal/TCPServer.hh"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace SpwRmap::internal {

inline auto set_listening_sockopt(int fd) -> void {
  // Allow quick rebinding after restart.
  int yes = 1;
  (void)::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#ifdef SO_REUSEPORT
  (void)::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes));
#endif
  // CLOEXEC for listen fd as well.
  const int fdflags = ::fcntl(fd, F_GETFD);
  if (fdflags < 0 || ::fcntl(fd, F_SETFD, fdflags | FD_CLOEXEC) < 0) {
    throw std::system_error(errno, std::system_category(), "fcntl(FD_CLOEXEC)");
  }
}

inline auto server_set_sockopts(int fd) -> void {
  int yes = 1;
  // Disable Nagle for latency-sensitive traffic.
  if (::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes)) != 0) {
    throw std::system_error(errno, std::system_category(), "setsockopt(TCP_NODELAY)");
  }
#ifdef __APPLE__
  // Avoid SIGPIPE on write-side errors.
  if (::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(yes)) != 0) {
    throw std::system_error(errno, std::system_category(), "setsockopt(SO_NOSIGPIPE)");
  }
#endif
  // Ensure close-on-exec (harmless if already set).
  const int fdflags = ::fcntl(fd, F_GETFD);
  if (fdflags < 0 || ::fcntl(fd, F_SETFD, fdflags | FD_CLOEXEC) < 0) {
    throw std::system_error(errno, std::system_category(), "fcntl(FD_CLOEXEC)");
  }
}

auto TCPServer::close_retry_(int& fd) noexcept -> void {
  if (fd < 0) {
    return;
  }
  int r = 0;
  do {
    r = ::close(fd);
  } while (r < 0 && errno == EINTR);
  fd = -1;
}

TCPServer::TCPServer(std::string_view bind_address, uint32_t port,
                     std::chrono::microseconds send_timeout,
                     std::chrono::microseconds recv_timeout) {
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;  // IPv4/IPv6 both
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = AI_PASSIVE;  // for bind

  addrinfo* res = nullptr;
  const std::string serv = std::to_string(port);
  if (int rc = ::getaddrinfo(std::string(bind_address).c_str(), serv.c_str(), &hints, &res);
      rc != 0) {
    throw std::runtime_error(std::string("getaddrinfo: ") + gai_strerror(rc));
  }

  std::system_error last{EINVAL, std::system_category(), "no address succeeded"};

  for (addrinfo* ai = res; ai != nullptr; ai = ai->ai_next) {
    listen_fd_ = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (listen_fd_ < 0) {
      last = {errno, std::system_category(), "socket"};
      continue;
    }

    try {
      internal::set_listening_sockopt(listen_fd_);

      if (::bind(listen_fd_, ai->ai_addr, ai->ai_addrlen) != 0) {
        last = {errno, std::system_category(), "bind"};
        close_retry_(listen_fd_);
        continue;
      }
      if (::listen(listen_fd_, SOMAXCONN) != 0) {
        last = {errno, std::system_category(), "listen"};
        close_retry_(listen_fd_);
        continue;
      }

      for (;;) {
        client_fd_ = ::accept(listen_fd_, nullptr, nullptr);
        if (client_fd_ < 0 && errno == EINTR) {
          continue;
        }
        break;
      }
      if (client_fd_ < 0) {
        last = {errno, std::system_category(), "accept"};
        close_retry_(listen_fd_);
        continue;
      }

      internal::server_set_sockopts(client_fd_);
      setSendTimeout(send_timeout);
      setRecvTimeout(recv_timeout);
      break;
    } catch (const std::system_error& e) {
      last = e;
      close_retry_(listen_fd_);
      if (client_fd_ >= 0) {
        close_retry_(client_fd_);
      }
    }
  }

  ::freeaddrinfo(res);
  if (client_fd_ < 0) {
    client_fd_ = -1;
    throw last;
  }

  // Close the listen fd as we have accepted a client.
  if (listen_fd_ >= 0) {
    close_retry_(listen_fd_);
  }
}
TCPServer::~TCPServer() {
  close_retry_(client_fd_);
  close_retry_(listen_fd_);
}
auto TCPServer::setRecvTimeout(std::chrono::microseconds timeout) -> void {
  if (timeout < std::chrono::microseconds::zero()) {
    throw std::invalid_argument("SO_RCVTIMEO: negative timeout");
  }

  const auto tv_sec =
      static_cast<time_t>(std::chrono::duration_cast<std::chrono::seconds>(timeout).count());
  const auto tv_usec = static_cast<suseconds_t>(timeout.count() % 1000000);

  timeval tv{};
  tv.tv_sec = tv_sec;
  tv.tv_usec = tv_usec;

  if (::setsockopt(client_fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0) {
    throw std::system_error(errno, std::system_category(), "setsockopt(SO_RCVTIMEO)");
  }
}
auto TCPServer::setSendTimeout(std::chrono::microseconds timeout) -> void {
  if (timeout < std::chrono::microseconds::zero()) {
    throw std::invalid_argument("SO_SNDTIMEO: negative timeout");
  }
  const auto tv_sec =
      static_cast<time_t>(std::chrono::duration_cast<std::chrono::seconds>(timeout).count());
  const auto tv_usec = static_cast<suseconds_t>(timeout.count() % 1000000);

  timeval tv{};
  tv.tv_sec = tv_sec;
  tv.tv_usec = tv_usec;

  if (::setsockopt(client_fd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0) {
    throw std::system_error(errno, std::system_category(), "setsockopt(SO_SNDTIMEO)");
  }
}
auto TCPServer::send_all(std::span<const uint8_t> data) -> void {
  while (!data.empty()) {
#ifndef __APPLE__
    constexpr int kFlags = MSG_NOSIGNAL;
#else
    constexpr int kFlags = 0;  // SO_NOSIGPIPE is set in set_sockopts()
#endif
    const ssize_t n = ::send(client_fd_, data.data(), data.size(), kFlags);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        throw std::runtime_error("send: timeout");
      }
      throw std::system_error{errno, std::system_category(), "send"};
    }
    if (n == 0) {
      continue;  // not EOF for send(); retry
    }
    data = data.subspan(static_cast<std::size_t>(n));
  }
}
auto TCPServer::recv_some(std::span<uint8_t> buf) -> std::size_t {
  if (buf.empty()) {
    return 0U;
  }
  for (;;) {
    const ssize_t n = ::recv(client_fd_, buf.data(), buf.size(), 0);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        throw std::runtime_error("recv: timeout");
      }
      throw std::system_error{errno, std::system_category(), "recv"};
    }
    return static_cast<std::size_t>(n);  // 0 -> EOF
  }
}
};  // namespace SpwRmap::internal
