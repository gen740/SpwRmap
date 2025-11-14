#include "spw_rmap/internal/tcp_client.hh"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/fcntl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <span>
#include <system_error>

namespace spw_rmap::internal {

using namespace std::chrono_literals;

auto connect_with_timeout_(const int fd, const sockaddr* addr,
                           socklen_t addrlen,
                           std::chrono::microseconds timeout) noexcept
    -> std::expected<std::monostate, std::error_code> {
  if (timeout < std::chrono::microseconds::zero()) {
    return std::unexpected{std::make_error_code(std::errc::invalid_argument)};
  }
  const auto ms64 =
      std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count();
  const int ms =
      ms64 > static_cast<long long>(std::numeric_limits<int32_t>::max())
          ? std::numeric_limits<int32_t>::max()
          : static_cast<int>(ms64);

  const int oldfl = ::fcntl(fd, F_GETFL);
  if (oldfl < 0) {
    return std::unexpected{std::error_code(errno, std::system_category())};
  }
  const bool was_blocking = (oldfl & O_NONBLOCK) == 0;
  if (was_blocking) {
    if (::fcntl(fd, F_SETFL, oldfl | O_NONBLOCK) < 0) {
      return std::unexpected{std::error_code(errno, std::system_category())};
    }
  }

  struct Restore {
    Restore(const Restore&) = delete;
    Restore(Restore&&) = delete;
    auto operator=(const Restore&) -> Restore& = delete;
    auto operator=(Restore&&) -> Restore& = delete;
    Restore(int fd, int fl, bool on) : fd(fd), fl(fl), on(on) {}
    int fd;
    int fl;
    bool on;
    ~Restore() {
      if (on) {
        (void)::fcntl(fd, F_SETFL, fl);
      }
    }
  } restore{fd, oldfl, was_blocking};

  int rc = 0;
  do {
    rc = ::connect(fd, addr, addrlen);
  } while (rc != 0 && errno == EINTR);

  if (rc == 0) {
    return {};  // Connected immediately.
  }

  if (errno != EINPROGRESS) {
    return std::unexpected{std::error_code(errno, std::system_category())};
  }
  pollfd pfd{.fd = fd, .events = POLLOUT, .revents = 0};
  int prc = 0;
  do {
    prc = ::poll(&pfd, 1, ms);
  } while (prc < 0 && errno == EINTR);

  if (prc == 0) {
    return std::unexpected{std::error_code(ETIMEDOUT, std::system_category())};
  }
  if (prc < 0) {
    return std::unexpected{std::error_code(errno, std::system_category())};
  }

  int soerr = 0;
  auto slen = static_cast<socklen_t>(sizeof(soerr));
  if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &slen) != 0) {
    return std::unexpected{std::error_code(errno, std::system_category())};
  }
  if (soerr != 0) {
    return std::unexpected{std::error_code(soerr, std::system_category())};
  }
  return {};
}

auto set_sockopts(int fd) noexcept
    -> std::expected<std::monostate, std::error_code> {
  int yes = 1;
  if (::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes)) != 0) {
    return std::unexpected{std::error_code(errno, std::system_category())};
  }
#ifdef __APPLE__
  if (::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(yes)) != 0) {
    return std::unexpected{std::error_code(errno, std::system_category())};
  }
#endif
  const int fdflags = ::fcntl(fd, F_GETFD);
  if (fdflags < 0 || ::fcntl(fd, F_SETFD, fdflags | FD_CLOEXEC) < 0) {
    return std::unexpected{std::error_code(errno, std::system_category())};
  }
  return {};
}

auto TCPClient::close_retry_(int fd) noexcept -> void {
  if (fd >= 0) {
    auto r = 0;
    do {
      r = ::close(fd);
    } while (r < 0 && errno == EINTR);
  }
}

TCPClient::~TCPClient() {
  disconnect();
  fd_ = -1;
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

[[nodiscard]] auto TCPClient::connect(
    std::chrono::microseconds recv_timeout,
    std::chrono::microseconds send_timeout,
    std::chrono::microseconds connect_timeout) noexcept
    -> std::expected<std::monostate, std::error_code> {
  if (fd_ >= 0) {
    return std::unexpected{std::make_error_code(std::errc::already_connected)};
  }
  addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  addrinfo* res = nullptr;
  if (int rc = ::getaddrinfo(std::string(ip_address_).c_str(),
                             std::string(port_).c_str(), &hints, &res);
      rc != 0) {
    if (rc == EAI_SYSTEM) {
      return std::unexpected{std::error_code(errno, std::system_category())};
    } else {
      return std::unexpected{std::error_code(rc, gai_category())};
    }
  }

  std::expected<std::monostate, std::error_code> last =
      std::unexpected(std::make_error_code(std::errc::invalid_argument));

  for (addrinfo* ai = res; ai != nullptr; ai = ai->ai_next) {
    fd_ = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (fd_ < 0) {
      last = std::unexpected{std::error_code(errno, std::system_category())};
      close_retry_(fd_);
      fd_ = -1;
      continue;
    }
    last = internal::set_sockopts(fd_)
               .and_then([this, send_timeout](auto) -> auto {
                 return setSendTimeout(send_timeout);
               })
               .and_then([this, recv_timeout](auto) -> auto {
                 return setRecvTimeout(recv_timeout);
               })
               .and_then([this, connect_timeout, ai](auto) -> auto {
                 return connect_with_timeout_(fd_, ai->ai_addr, ai->ai_addrlen,
                                              connect_timeout);
               });
    if (!last.has_value()) {
      close_retry_(fd_);
      fd_ = -1;
      continue;
    }
    break;  // success
  }

  ::freeaddrinfo(res);
  if (fd_ < 0) {
    fd_ = -1;
  }
  return last;
}

auto TCPClient::disconnect() noexcept -> void {
  close_retry_(fd_);
  fd_ = -1;
}

auto TCPClient::reconnect(std::chrono::microseconds recv_timeout,
                          std::chrono::microseconds send_timeout,
                          std::chrono::microseconds connect_timeout) noexcept
    -> std::expected<std::monostate, std::error_code> {
  disconnect();
  fd_ = -1;
  return connect(recv_timeout, send_timeout, connect_timeout);
}

auto TCPClient::setRecvTimeout(std::chrono::microseconds timeout) noexcept
    -> std::expected<std::monostate, std::error_code> {
  const auto tv_sec = static_cast<time_t>(
      std::chrono::duration_cast<std::chrono::seconds>(timeout).count());
  const auto tv_usec = static_cast<suseconds_t>(timeout.count() % 1000000);

  timeval tv{};
  tv.tv_sec = tv_sec;
  tv.tv_usec = tv_usec;

  if (::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0) {
    return std::unexpected{std::error_code(errno, std::system_category())};
  }
  return {};
}

auto TCPClient::setSendTimeout(std::chrono::microseconds timeout) noexcept
    -> std::expected<std::monostate, std::error_code> {
  if (timeout < std::chrono::microseconds::zero()) {
    return std::unexpected{std::make_error_code(std::errc::invalid_argument)};
  }
  const auto tv_sec = static_cast<time_t>(
      std::chrono::duration_cast<std::chrono::seconds>(timeout).count());
  const auto tv_usec = static_cast<suseconds_t>(timeout.count() % 1000000);

  timeval tv{};
  tv.tv_sec = tv_sec;
  tv.tv_usec = tv_usec;

  if (::setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0) {
    return std::unexpected{std::error_code(errno, std::system_category())};
  }
  return {};
}

auto TCPClient::sendAll(std::span<const uint8_t> data) noexcept
    -> std::expected<std::monostate, std::error_code> {
  if (fd_ < 0) {
    return std::unexpected{std::make_error_code(std::errc::not_connected)};
  }
  // bool retried_zero = false;
  while (!data.empty()) {
#ifndef __APPLE__
    constexpr int kFlags = MSG_NOSIGNAL;
#else
    constexpr int kFlags = 0;
#endif
    const ssize_t n = ::send(fd_, data.data(), data.size(), kFlags);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return std::unexpected{std::make_error_code(std::errc::timed_out)};
      }
      return std::unexpected{std::error_code(errno, std::system_category())};
    }
    if (n == 0) {
      // if (retried_zero) {
      //   return std::unexpected{std::make_error_code(std::errc::io_error)};
      // }
      // pollfd pfd{.fd = fd_, .events = POLLOUT, .revents = 0};
      // int prc = 0;
      // do {
      //   prc = ::poll(&pfd, 1, 10);
      // } while (prc < 0 && errno == EINTR);
      //
      // if (prc == 0) {
      //   return std::unexpected{std::make_error_code(std::errc::timed_out)};
      // }
      // if (prc < 0) {
      //   return std::unexpected{std::error_code(errno,
      //   std::system_category())};
      // }
      // if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
      //   return std::unexpected{
      //       std::make_error_code(std::errc::connection_aborted)};
      // }
      // if ((pfd.revents & POLLOUT) == 0) {
      //   return std::unexpected{std::make_error_code(std::errc::io_error)};
      // }
      // retried_zero = true;
      continue;
    }
    data = data.subspan(static_cast<size_t>(n));
  }
  return {};
}

auto TCPClient::recvSome(std::span<uint8_t> buf) noexcept
    -> std::expected<size_t, std::error_code> {
  if (buf.empty()) {
    return 0U;  // Nothing to receive
  }
  for (;;) {
    const ssize_t n = ::recv(fd_, buf.data(), buf.size(), 0);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return std::unexpected{std::make_error_code(std::errc::timed_out)};
      }
      return std::unexpected{std::error_code(errno, std::system_category())};
      // } else if (n == 0) {
      //   return std::unexpected{std::make_error_code(std::errc::io_error)};
    }
    return static_cast<size_t>(n);
  }
}

}  // namespace spw_rmap::internal
