#include <pybind11/chrono.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <SpwRmap/SpwRmapNodeBase.hh>
#include <SpwRmap/SpwRmapTCPNode.hh>
#include <SpwRmap/TargetNode.hh>
#include <utility>

#include "span_caster.hh"

namespace py = pybind11;

using namespace std::chrono_literals;

struct PyTargetNodeBase : public SpwRmap::TargetNodeBase {
  using SpwRmap::TargetNodeBase::TargetNodeBase;

  [[nodiscard]] auto getTargetSpaceWireAddress() const noexcept
      -> std::span<const uint8_t> override {
    PYBIND11_OVERRIDE_PURE(std::span<const uint8_t>, SpwRmap::TargetNodeBase,
                           getTargetSpaceWireAddress);
  }

  [[nodiscard]] auto getReplyAddress() const noexcept
      -> std::span<const uint8_t> override {
    PYBIND11_OVERRIDE_PURE(std::span<const uint8_t>, SpwRmap::TargetNodeBase,
                           getReplyAddress);
  }
};

struct PySpwRmapNodeBase : public SpwRmap::SpwRmapNodeBase {
  using SpwRmap::SpwRmapNodeBase::SpwRmapNodeBase;

  [[nodiscard]] auto write(const SpwRmap::TargetNodeBase&, uint32_t,
                           const std::span<const uint8_t>) noexcept
      -> std::expected<std::monostate, std::error_code> override {
    using R = std::expected<std::monostate, std::error_code>;
    PYBIND11_OVERRIDE_PURE(R, SpwRmap::SpwRmapNodeBase, write);
  }

  [[nodiscard]] auto read(const SpwRmap::TargetNodeBase&, uint32_t,
                          const std::span<uint8_t>) noexcept
      -> std::expected<std::monostate, std::error_code> override {
    using R = std::expected<std::monostate, std::error_code>;
    PYBIND11_OVERRIDE_PURE(R, SpwRmap::SpwRmapNodeBase, read);
  }

  [[nodiscard]] auto emitTimeCode(uint8_t) noexcept
      -> std::expected<std::monostate, std::error_code> override {
    using R = std::expected<std::monostate, std::error_code>;
    PYBIND11_OVERRIDE_PURE(R, SpwRmap::SpwRmapNodeBase, emitTimeCode);
  }
};

template <class T>
[[nodiscard]] auto unwrap_or_throw(std::expected<T, std::error_code> r) -> T {
  if (!r) {
    throw std::system_error(r.error());
  }
  return *r;
}

inline auto unwrap_or_throw(std::expected<std::monostate, std::error_code> r)
    -> void {
  if (!r) {
    throw std::system_error(r.error());
  }
}

struct PySpwRmapTCPNodeConfig {
  SpwRmap::SpwRmapTCPNodeConfig cfg{};
  std::string ip_storage;

  PySpwRmapTCPNodeConfig(
      std::string ip, std::string port, size_t send_sz = 4096,
      size_t recv_sz = 4096,
      SpwRmap::BufferPolicy policy = SpwRmap::BufferPolicy::AutoResize)
      : ip_storage(std::move(ip)) {
    cfg.ip_address = ip_storage;
    cfg.port = port;
    cfg.send_buffer_size = send_sz;
    cfg.recv_buffer_size = recv_sz;
    cfg.buffer_policy = policy;
  }

  void set_ip(const std::string& ip) {
    ip_storage = ip;
    cfg.ip_address = ip_storage;
  }
  [[nodiscard]] auto get_ip() const -> const std::string& { return ip_storage; }

  void set_port(std::string p) { cfg.port = p; }
  [[nodiscard]] auto get_port() const -> std::string { return cfg.port; }

  void set_send(size_t n) { cfg.send_buffer_size = n; }
  [[nodiscard]] auto get_send() const -> size_t { return cfg.send_buffer_size; }

  void set_recv(size_t n) { cfg.recv_buffer_size = n; }
  [[nodiscard]] auto get_recv() const -> size_t { return cfg.recv_buffer_size; }

  void set_policy(SpwRmap::BufferPolicy p) { cfg.buffer_policy = p; }
  [[nodiscard]] auto get_policy() const -> SpwRmap::BufferPolicy {
    return cfg.buffer_policy;
  }

  [[nodiscard]] auto get_cfg() const -> SpwRmap::SpwRmapTCPNodeConfig {
    return cfg;
  }
};

PYBIND11_MODULE(_core, m) {
  py::class_<SpwRmap::TargetNodeBase, PyTargetNodeBase>(m, "_TargetNodeBase")
      .def(py::init<uint8_t, uint8_t>(), py::arg("logical_address") = 0x00,
           py::arg("initiator_logical_address") = 0xFE)
      .def("get_target_logical_address",
           &SpwRmap::TargetNodeBase::getTargetLogicalAddress)
      .def("get_initiator_logical_address",
           &SpwRmap::TargetNodeBase::getInitiatorLogicalAddress)
      .def("get_target_spacewire_address",
           &SpwRmap::TargetNodeBase::getTargetSpaceWireAddress)
      .def("get_reply_address", &SpwRmap::TargetNodeBase::getReplyAddress);

  py::class_<SpwRmap::TargetNodeDynamic, SpwRmap::TargetNodeBase>(m,
                                                                  "TargetNode")
      .def(py::init<uint8_t, std::vector<uint8_t>, std::vector<uint8_t>,
                    uint8_t>(),
           py::arg("logical_address") = 0x00,
           py::arg("target_spacewire_address") = std::vector<uint8_t>(),
           py::arg("reply_address") = std::vector<uint8_t>(),
           py::arg("initiator_logical_address") = 0xFE)
      .def("get_target_spacewire_address",
           &SpwRmap::TargetNodeDynamic::getTargetSpaceWireAddress)
      .def("get_reply_address", &SpwRmap::TargetNodeDynamic::getReplyAddress)
      .def("get_target_logical_address",
           &SpwRmap::TargetNodeDynamic::getTargetLogicalAddress)
      .def("get_initiator_logical_address",
           &SpwRmap::TargetNodeDynamic::getInitiatorLogicalAddress);

  py::class_<SpwRmap::SpwRmapNodeBase, PySpwRmapNodeBase>(m, "_SpwRmapNodeBase")
      .def(py::init<>())
      .def(
          "write",
          [](SpwRmap::SpwRmapNodeBase& self,
             const SpwRmap::TargetNodeBase& target_node,
             std::uint32_t memory_address,
             std::span<const std::uint8_t> data) -> void {
            return unwrap_or_throw(
                self.write(target_node, memory_address, data));
          },
          py::arg("target_node"), py::arg("memory_address"), py::arg("data"))
      .def(
          "read",
          [](SpwRmap::SpwRmapNodeBase& self,
             const SpwRmap::TargetNodeBase& target_node,
             std::uint32_t memory_address,
             std::span<std::uint8_t> data) -> void {
            return unwrap_or_throw(
                self.read(target_node, memory_address, data));
          },
          py::arg("target_node"), py::arg("memory_address"), py::arg("data"))
      .def(
          "emit_time_code",
          [](SpwRmap::SpwRmapNodeBase& self, std::uint8_t tc) -> void {
            unwrap_or_throw(self.emitTimeCode(tc));
          },
          py::arg("timecode"));

  py::enum_<SpwRmap::BufferPolicy>(m, "SpwRmapBufferPolicy")
      .value("Fixed", SpwRmap::BufferPolicy::Fixed)
      .value("AutoResize", SpwRmap::BufferPolicy::AutoResize)
      .export_values();

  py::class_<SpwRmap::SpwRmapTCPNode, SpwRmap::SpwRmapNodeBase>(
      m, "SpwRmapTCPNode")
      .def(py::init([](const std::string& ip, const std::string& port,
                       size_t send_sz, size_t recv_sz,
                       SpwRmap::BufferPolicy policy)
                        -> std::unique_ptr<SpwRmap::SpwRmapTCPNode> {
             SpwRmap::SpwRmapTCPNodeConfig cfg{.ip_address = ip,
                                               .port = port,
                                               .send_buffer_size = send_sz,
                                               .recv_buffer_size = recv_sz,
                                               .buffer_policy = policy};
             return std::make_unique<SpwRmap::SpwRmapTCPNode>(cfg);
           }),
           py::arg("ip_address"), py::arg("port"),
           py::arg_v("send_buffer_size", 4096),
           py::arg_v("recv_buffer_size", 4096),
           py::arg_v("buffer_policy", SpwRmap::BufferPolicy::AutoResize,
                     "SpwRmapBufferPolicy.AutoResize"))
      .def(
          "connect",
          [](SpwRmap::SpwRmapTCPNode& self,
             std::chrono::microseconds recv_timeout,
             std::chrono::microseconds send_timeout,
             std::chrono::microseconds connect_timeout) -> void {
            unwrap_or_throw(
                self.connect(recv_timeout, send_timeout, connect_timeout));
          },
          py::arg("recv_timeout") = 100ms, py::arg("send_timeout") = 100ms,
          py::arg("connect_timeout") = 100ms)
      .def(
          "write",
          [](SpwRmap::SpwRmapTCPNode& self,
             const SpwRmap::TargetNodeBase& target_node,
             std::uint32_t memory_address,
             std::span<const std::uint8_t> data) -> void {
            return unwrap_or_throw(
                self.write(target_node, memory_address, data));
          },
          py::arg("target_node"), py::arg("memory_address"), py::arg("data"))
      .def(
          "read",
          [](SpwRmap::SpwRmapTCPNode& self,
             const SpwRmap::TargetNodeBase& target_node,
             std::uint32_t memory_address,
             std::span<std::uint8_t> data) -> void {
            return unwrap_or_throw(
                self.read(target_node, memory_address, data));
          },
          py::arg("target_node"), py::arg("memory_address"), py::arg("data"))
      .def(
          "emit_time_code",
          [](SpwRmap::SpwRmapTCPNode& self, std::uint8_t tc) -> void {
            unwrap_or_throw(self.emitTimeCode(tc));
          },
          py::arg("timecode"));
}
