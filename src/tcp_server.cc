#include "spw_rmap/internal/tcp_server.hh"

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
#include <string>
#include <system_error>

#include "spw_rmap/internal/debug.hh"

namespace spw_rmap::internal {

static inline auto set_listening_sockopt(int fd)
    -> std::expected<std::monostate, std::error_code> {
  // Allow quick rebinding after restart.
  int yes = 1;
  (void)::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#ifdef SO_REUSEPORT
  (void)::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes));
#endif
  // CLOEXEC for listen fd as well.
  const int fdflags = ::fcntl(fd, F_GETFD);
  if (fdflags < 0 || ::fcntl(fd, F_SETFD, fdflags | FD_CLOEXEC) < 0) {
    spw_rmap::debug::debug("Failed to set FD_CLOEXEC on listening socket");
    return std::unexpected{std::error_code(errno, std::system_category())};
  }
  return {};
}

static inline auto server_set_sockopts(int fd)
    -> std::expected<std::monostate, std::error_code> {
  int yes = 1;
  // Disable Nagle for latency-sensitive traffic.
  if (::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes)) != 0) {
    spw_rmap::debug::debug("Failed to set TCP_NODELAY");
    return std::unexpected{std::error_code(errno, std::system_category())};
  }
#ifdef __APPLE__
  // Avoid SIGPIPE on write-side errors.
  if (::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(yes)) != 0) {
    spw_rmap::debug::debug("Failed to set SO_NOSIGPIPE");
    return std::unexpected{std::error_code(errno, std::system_category())};
  }
#endif
  // Ensure close-on-exec (harmless if already set).
  const int fdflags = ::fcntl(fd, F_GETFD);
  if (fdflags < 0 || ::fcntl(fd, F_SETFD, fdflags | FD_CLOEXEC) < 0) {
    spw_rmap::debug::debug("Failed to set FD_CLOEXEC on server socket");
    return std::unexpected{std::error_code(errno, std::system_category())};
  }
  return {};
}

auto TCPServer::close_retry_(int fd) noexcept -> void {
  if (fd < 0) {
    return;
  }
  int r = 0;
  do {
    r = ::close(fd);
  } while (r < 0 && errno == EINTR);
}

struct gai_category_t final : std::error_category {
  [[nodiscard]] auto name() const noexcept -> const char* override {
    return "gai";
  }
  [[nodiscard]] auto message(int ev) const -> std::string override {
    return ::gai_strerror(ev);
  }
};

static inline auto gai_category() noexcept -> const std::error_category& {
  static const gai_category_t cat{};
  return cat;
}

auto TCPServer::accept_once() noexcept
    -> std::expected<std::monostate, std::error_code> {
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;  // IPv4/IPv6 both
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = AI_PASSIVE;  // for bind

  addrinfo* res = nullptr;
  if (int rc = ::getaddrinfo(std::string(bind_address_).c_str(),
                             std::string(port_).c_str(), &hints, &res);
      rc != 0) {
    spw_rmap::debug::debug("getaddrinfo error: ", ::gai_strerror(rc));
    return std::unexpected{std::error_code(rc, gai_category())};
  }
  std::expected<std::monostate, std::error_code> last =
      std::unexpected(std::make_error_code(std::errc::invalid_argument));

  for (addrinfo* ai = res; ai != nullptr; ai = ai->ai_next) {
    listen_fd_ = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (listen_fd_ < 0) {
      spw_rmap::debug::debug("Failed to create listening socket");
      last = std::unexpected{std::error_code(errno, std::system_category())};
      continue;
    }

    last = internal::set_listening_sockopt(listen_fd_);
    if (!last.has_value()) {
      close_retry_(listen_fd_);
      listen_fd_ = -1;
      continue;
    }
    if (::bind(listen_fd_, ai->ai_addr, ai->ai_addrlen) != 0) {
      last = std::unexpected{std::error_code(errno, std::system_category())};
      close_retry_(listen_fd_);
      listen_fd_ = -1;
      continue;
    }
    if (::listen(listen_fd_, SOMAXCONN) != 0) {
      last = std::unexpected{std::error_code(errno, std::system_category())};
      close_retry_(listen_fd_);
      listen_fd_ = -1;
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
      last = std::unexpected{std::error_code(errno, std::system_category())};
      close_retry_(listen_fd_);
      listen_fd_ = -1;
      continue;
    }

    last = internal::server_set_sockopts(client_fd_)
               .or_else([this](const auto& ec)
                            -> std::expected<std::monostate, std::error_code> {
                 close_retry_(listen_fd_);
                 listen_fd_ = -1;
                 close_retry_(client_fd_);
                 client_fd_ = -1;
                 spw_rmap::debug::debug(
                     "Failed to set socket options on accepted socket");
                 return std::unexpected{ec};
               });
  }
  ::freeaddrinfo(res);
  if (client_fd_ < 0) {
    client_fd_ = -1;
    return last;
  }
  close_retry_(listen_fd_);
  listen_fd_ = -1;
  return {};
}

TCPServer::~TCPServer() noexcept {
  close_retry_(client_fd_);
  client_fd_ = -1;
  close_retry_(listen_fd_);
  listen_fd_ = -1;
}

auto TCPServer::setRecvTimeout(std::chrono::microseconds timeout) noexcept
    -> std::expected<std::monostate, std::error_code> {
  if (timeout < std::chrono::microseconds::zero()) {
    spw_rmap::debug::debug("Negative timeout value");
    return std::unexpected{std::make_error_code(std::errc::invalid_argument)};
  }

  const auto tv_sec = static_cast<time_t>(
      std::chrono::duration_cast<std::chrono::seconds>(timeout).count());
  const auto tv_usec = static_cast<suseconds_t>(timeout.count() % 1000000);

  timeval tv{};
  tv.tv_sec = tv_sec;
  tv.tv_usec = tv_usec;

  if (::setsockopt(client_fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0) {
    spw_rmap::debug::debug("Failed to set receive timeout");
    return std::unexpected{std::error_code(errno, std::system_category())};
  }
  return {};
}

auto TCPServer::setSendTimeout(std::chrono::microseconds timeout) noexcept
    -> std::expected<std::monostate, std::error_code> {
  if (timeout < std::chrono::microseconds::zero()) {
    spw_rmap::debug::debug("Negative timeout value");
    return std::unexpected{std::make_error_code(std::errc::invalid_argument)};
  }
  const auto tv_sec = static_cast<time_t>(
      std::chrono::duration_cast<std::chrono::seconds>(timeout).count());
  const auto tv_usec = static_cast<suseconds_t>(timeout.count() % 1000000);

  timeval tv{};
  tv.tv_sec = tv_sec;
  tv.tv_usec = tv_usec;

  if (::setsockopt(client_fd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0) {
    spw_rmap::debug::debug("Failed to set send timeout");
    return std::unexpected{std::error_code(errno, std::system_category())};
  }
  return {};
}

auto TCPServer::sendAll(std::span<const uint8_t> data) noexcept
    -> std::expected<std::monostate, std::error_code> {
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
        spw_rmap::debug::debug("Send would block, timing out");
        return std::unexpected{std::make_error_code(std::errc::timed_out)};
      }
      spw_rmap::debug::debug("Send failed");
      return std::unexpected{std::error_code(errno, std::system_category())};
    }
    if (n == 0) {
      continue;  // not EOF for send(); retry
    }
    data = data.subspan(static_cast<std::size_t>(n));
  }
  return {};
}

auto TCPServer::recvSome(std::span<uint8_t> buf) noexcept
    -> std::expected<size_t, std::error_code> {
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
        spw_rmap::debug::debug("Receive would block, timing out");
        return std::unexpected{std::make_error_code(std::errc::timed_out)};
      }
      spw_rmap::debug::debug("Receive failed");
      return std::unexpected{std::error_code(errno, std::system_category())};
    }
    return static_cast<std::size_t>(n);  // 0 -> EOF
  }
}

auto TCPServer::shutdown() noexcept
    -> std::expected<std::monostate, std::error_code> {
  if (client_fd_ < 0) {
    spw_rmap::debug::debug("Client socket not connected");
    return std::unexpected(
        std::make_error_code(std::errc::bad_file_descriptor));
  }
  if (::shutdown(client_fd_, SHUT_RDWR) < 0) {
    spw_rmap::debug::debug("Failed to shutdown client socket");
    return std::unexpected(std::error_code(errno, std::generic_category()));
  }
  return std::monostate{};
}

auto TCPServer::closeClient() noexcept
    -> std::expected<std::monostate, std::error_code> {
  if (client_fd_ < 0) {
    spw_rmap::debug::debug("Client socket not connected");
    return std::unexpected(
        std::make_error_code(std::errc::bad_file_descriptor));
  }

  int r = 0;
  do {
    r = ::close(client_fd_);
  } while (r < 0 && errno == EINTR);

  if (r < 0) {
    spw_rmap::debug::debug("Failed to close client socket");
    return std::unexpected(std::error_code(errno, std::system_category()));
  }

  client_fd_ = -1;
  return {};
}

};  // namespace spw_rmap::internal
