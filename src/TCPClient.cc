#include "SpwRmap/internal/TCPClient.hh"

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

namespace SpwRmap::internal {

using namespace std::chrono_literals;

auto connect_with_timeout_(const int fd, const sockaddr* addr, socklen_t addrlen,
                           std::chrono::microseconds timeout) -> void {
  if (timeout < std::chrono::microseconds::zero()) {
    throw std::invalid_argument("connect_with_timeout_: negative timeout");
  }
  const int ms =
      static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count());

  const int oldfl = ::fcntl(fd, F_GETFL);
  if (oldfl < 0) {
    throw std::system_error(errno, std::system_category(), "fcntl(F_GETFL)");
  }
  const bool was_blocking = (oldfl & O_NONBLOCK) == 0;
  if (was_blocking) {
    if (::fcntl(fd, F_SETFL, oldfl | O_NONBLOCK) < 0) {
      throw std::system_error(errno, std::system_category(), "fcntl(F_SETFL, O_NONBLOCK)");
    }
  }

  struct Restore {
    int fd;
    int fl;
    bool on;
    ~Restore() {
      if (on) {
        (void)::fcntl(fd, F_SETFL, fl);
      }
    }
  } restore{.fd = fd, .fl = oldfl, .on = was_blocking};

  int rc = 0;
  do {
    rc = ::connect(fd, addr, addrlen);
  } while (rc != 0 && errno == EINTR);

  if (rc == 0) {
    return;  // Connected immediately.
  }

  if (errno != EINPROGRESS) {
    throw std::system_error(errno, std::system_category(), "connect");
  }
  pollfd pfd{.fd = fd, .events = POLLOUT, .revents = 0};
  int prc = 0;
  do {
    prc = ::poll(&pfd, 1, ms);
  } while (prc < 0 && errno == EINTR);

  if (prc == 0) {
    throw std::system_error(ETIMEDOUT, std::system_category(), "connect timeout");
  }
  if (prc < 0) {
    throw std::system_error(errno, std::system_category(), "poll(connect)");
  }

  int soerr = 0;
  auto slen = static_cast<socklen_t>(sizeof(soerr));
  if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &slen) != 0) {
    throw std::system_error(errno, std::system_category(), "getsockopt(SO_ERROR)");
  }
  if (soerr != 0) {
    throw std::system_error(soerr, std::system_category(), "connect");
  }
}

auto set_sockopts(int fd) -> void {
  int yes = 1;
  if (::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes)) != 0) {
    throw std::system_error(errno, std::system_category(), "setsockopt(TCP_NODELAY)");
  }
#ifdef __APPLE__
  if (::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(yes)) != 0) {
    throw std::system_error(errno, std::system_category(), "setsockopt(SO_NOSIGPIPE)");
  }
#endif
  const int fdflags = ::fcntl(fd, F_GETFD);
  if (fdflags < 0 || ::fcntl(fd, F_SETFD, fdflags | FD_CLOEXEC) < 0) {
    throw std::system_error(errno, std::system_category(), "fcntl(FD_CLOEXEC)");
  }
}

auto TCPClient::close_retry_(int& fd) -> void {
  if (fd >= 0) {
    auto r = 0;
    do {
      r = ::close(fd);
    } while (r < 0 && errno == EINTR);
    fd = -1;
  }
}

TCPClient::TCPClient(std::string_view ip_address, uint32_t port,
                     std::chrono::microseconds recv_timeout, std::chrono::microseconds send_timeout,
                     std::chrono::microseconds connect_timeout) {
  addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  std::string port_str = std::to_string(port);
  addrinfo* res = nullptr;
  if (int rc = ::getaddrinfo(std::string(ip_address).c_str(), port_str.c_str(), &hints, &res);
      rc != 0) {
    throw std::runtime_error(std::string("getaddrinfo: ") + gai_strerror(rc));
  }

  std::system_error last{EINVAL, std::system_category(), "no address succeeded"};

  for (addrinfo* ai = res; ai != nullptr; ai = ai->ai_next) {
    fd_ = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (fd_ < 0) {
      last = {errno, std::system_category(), "socket"};
      continue;
    }
    try {
      internal::set_sockopts(fd_);
      setSendTimeout(send_timeout);
      setRecvTimeout(recv_timeout);
      connect_with_timeout_(fd_, ai->ai_addr, ai->ai_addrlen, connect_timeout);
      break;  // success
    } catch (const std::system_error& e) {
      last = e;
      close_retry_(fd_);
      fd_ = -1;
    }
  }

  ::freeaddrinfo(res);
  if (fd_ < 0) {
    fd_ = -1;
    throw last;
  }
}
TCPClient::~TCPClient() { close_retry_(fd_); }
auto TCPClient::setRecvTimeout(std::chrono::microseconds timeout) -> void {
  if (timeout < std::chrono::microseconds::zero()) {
    throw std::invalid_argument("SO_RCVTIMEO: negative timeout");
  }

  const auto tv_sec =
      static_cast<time_t>(std::chrono::duration_cast<std::chrono::seconds>(timeout).count());
  const auto tv_usec = static_cast<suseconds_t>(timeout.count() % 1000000);

  timeval tv{};
  tv.tv_sec = tv_sec;
  tv.tv_usec = tv_usec;

  if (::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0) {
    throw std::system_error(errno, std::system_category(), "setsockopt(SO_RCVTIMEO)");
  }
}
auto TCPClient::setSendTimeout(std::chrono::microseconds timeout) -> void {
  if (timeout < std::chrono::microseconds::zero()) {
    throw std::invalid_argument("SO_SNDTIMEO: negative timeout");
  }
  const auto tv_sec =
      static_cast<time_t>(std::chrono::duration_cast<std::chrono::seconds>(timeout).count());
  const auto tv_usec = static_cast<suseconds_t>(timeout.count() % 1000000);

  timeval tv{};
  tv.tv_sec = tv_sec;
  tv.tv_usec = tv_usec;

  if (::setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0) {
    throw std::system_error(errno, std::system_category(), "setsockopt(SO_SNDTIMEO)");
  }
}
auto TCPClient::send_all(std::span<const uint8_t> data) -> void {
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
        throw std::runtime_error("send: timeout");
      }
      throw std::system_error{errno, std::system_category(), "send"};
    }
    if (n == 0) {
      // Not EOF for send(); treat as transient and retry.
      continue;
    }
    data = data.subspan(static_cast<size_t>(n));
  }
}
auto TCPClient::recv_some(std::span<uint8_t> buf) -> size_t {
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
        throw std::runtime_error("recv: timeout");
      }
      throw std::system_error{errno, std::system_category(), "recv"};
    }
    return static_cast<size_t>(n);
  }
}
}  // namespace SpwRmap::internal
