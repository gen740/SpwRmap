#include <pybind11/chrono.h>
#include <pybind11/stl.h>

#include <span>
#include <spw_rmap/spw_rmap_node_base.hh>
#include <spw_rmap/spw_rmap_tcp_node.hh>
#include <spw_rmap/target_node.hh>
#include <thread>

#include "span_caster.hh"

namespace py = pybind11;

using namespace std::chrono_literals;

struct PyTargetNodeBase : public spw_rmap::TargetNodeBase {
  using spw_rmap::TargetNodeBase::TargetNodeBase;

  [[nodiscard]] auto getTargetSpaceWireAddress() const noexcept
      -> std::span<const uint8_t> override {
    PYBIND11_OVERRIDE_PURE(std::span<const uint8_t>, spw_rmap::TargetNodeBase,
                           getTargetSpaceWireAddress);
  }

  [[nodiscard]] auto getReplyAddress() const noexcept
      -> std::span<const uint8_t> override {
    PYBIND11_OVERRIDE_PURE(std::span<const uint8_t>, spw_rmap::TargetNodeBase,
                           getReplyAddress);
  }
};

struct PySpwRmapNodeBase : public spw_rmap::SpwRmapNodeBase {
  using spw_rmap::SpwRmapNodeBase::SpwRmapNodeBase;

  auto poll() noexcept
      -> std::expected<std::monostate, std::error_code> override {
    using R = std::expected<std::monostate, std::error_code>;
    PYBIND11_OVERRIDE_PURE(R, spw_rmap::SpwRmapNodeBase, poll);
  }

  auto runLoop() noexcept
      -> std::expected<std::monostate, std::error_code> override {
    using R = std::expected<std::monostate, std::error_code>;
    PYBIND11_OVERRIDE_PURE(R, spw_rmap::SpwRmapNodeBase, runLoop);
  }

  auto registerOnWrite(std::function<void(spw_rmap::Packet)>) noexcept
      -> void override {
    PYBIND11_OVERRIDE_PURE(void, spw_rmap::SpwRmapNodeBase, registerOnWrite);
  }

  auto registerOnRead(
      std::function<std::vector<uint8_t>(spw_rmap::Packet)>) noexcept
      -> void override {
    PYBIND11_OVERRIDE_PURE(void, spw_rmap::SpwRmapNodeBase, registerOnRead);
  }

  [[nodiscard]] auto write(std::shared_ptr<spw_rmap::TargetNodeBase>, uint32_t,
                           const std::span<const uint8_t>) noexcept
      -> std::expected<std::monostate, std::error_code> override {
    using R = std::expected<std::monostate, std::error_code>;
    PYBIND11_OVERRIDE_PURE(R, spw_rmap::SpwRmapNodeBase, write);
  }

  [[nodiscard]] auto read(std::shared_ptr<spw_rmap::TargetNodeBase>, uint32_t,
                          const std::span<uint8_t>) noexcept
      -> std::expected<std::monostate, std::error_code> override {
    using R = std::expected<std::monostate, std::error_code>;
    PYBIND11_OVERRIDE_PURE(R, spw_rmap::SpwRmapNodeBase, read);
  }

  auto writeAsync(std::shared_ptr<spw_rmap::TargetNodeBase>, uint32_t,
                  const std::span<const uint8_t>,
                  std::function<void(spw_rmap::Packet)>) noexcept
      -> std::future<std::expected<std::monostate, std::error_code>> override {
    using R = std::future<std::expected<std::monostate, std::error_code>>;
    PYBIND11_OVERRIDE_PURE(R, spw_rmap::SpwRmapNodeBase, writeAsync);
  }

  auto readAsync(std::shared_ptr<spw_rmap::TargetNodeBase>, uint32_t, uint32_t,
                 std::function<void(spw_rmap::Packet)>) noexcept
      -> std::future<std::expected<std::monostate, std::error_code>> override {
    using R = std::future<std::expected<std::monostate, std::error_code>>;
    PYBIND11_OVERRIDE_PURE(R, spw_rmap::SpwRmapNodeBase, readAsync);
  }

  [[nodiscard]] auto emitTimeCode(uint8_t) noexcept
      -> std::expected<std::monostate, std::error_code> override {
    using R = std::expected<std::monostate, std::error_code>;
    PYBIND11_OVERRIDE_PURE(R, spw_rmap::SpwRmapNodeBase, emitTimeCode);
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
  spw_rmap::SpwRmapTCPNodeConfig cfg{};
  std::string ip_storage;

  PySpwRmapTCPNodeConfig(
      std::string ip, std::string port, size_t send_sz = 4096,
      size_t recv_sz = 4096,
      spw_rmap::BufferPolicy policy = spw_rmap::BufferPolicy::AutoResize)
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

  void set_policy(spw_rmap::BufferPolicy p) { cfg.buffer_policy = p; }
  [[nodiscard]] auto get_policy() const -> spw_rmap::BufferPolicy {
    return cfg.buffer_policy;
  }

  [[nodiscard]] auto get_cfg() const -> spw_rmap::SpwRmapTCPNodeConfig {
    return cfg;
  }
};

PYBIND11_MODULE(_core, m) {
  py::class_<spw_rmap::TargetNodeBase, PyTargetNodeBase>(m, "_TargetNodeBase")
      .def(py::init<uint8_t>(), py::arg("logical_address") = 0x00)
      .def("get_target_logical_address",
           &spw_rmap::TargetNodeBase::getTargetLogicalAddress)
      .def("get_target_spacewire_address",
           &spw_rmap::TargetNodeBase::getTargetSpaceWireAddress)
      .def("get_reply_address", &spw_rmap::TargetNodeBase::getReplyAddress);

  py::class_<spw_rmap::TargetNodeDynamic, spw_rmap::TargetNodeBase>(
      m, "TargetNode")
      .def(py::init<uint8_t, std::vector<uint8_t>, std::vector<uint8_t>>(),
           py::arg("logical_address") = 0x00,
           py::arg("target_spacewire_address") = std::vector<uint8_t>(),
           py::arg("reply_address") = std::vector<uint8_t>())
      .def("get_target_spacewire_address",
           &spw_rmap::TargetNodeDynamic::getTargetSpaceWireAddress)
      .def("get_reply_address", &spw_rmap::TargetNodeDynamic::getReplyAddress)
      .def("get_target_logical_address",
           &spw_rmap::TargetNodeDynamic::getTargetLogicalAddress);

  py::class_<spw_rmap::SpwRmapNodeBase, PySpwRmapNodeBase>(m,
                                                           "_SpwRmapNodeBase")
      .def(py::init<>())
      .def(
          "write",
          [](spw_rmap::SpwRmapNodeBase& self,
             std::shared_ptr<spw_rmap::TargetNodeBase> target_node,
             std::uint32_t memory_address,
             std::span<const std::uint8_t> data) -> void {
            return unwrap_or_throw(
                self.write(target_node, memory_address, data));
          },
          py::arg("target_node"), py::arg("memory_address"), py::arg("data"))
      .def(
          "read",
          [](spw_rmap::SpwRmapNodeBase& self,
             std::shared_ptr<spw_rmap::TargetNodeBase>& target_node,
             std::uint32_t memory_address,
             std::span<std::uint8_t> data) -> void {
            return unwrap_or_throw(
                self.read(target_node, memory_address, data));
          },
          py::arg("target_node"), py::arg("memory_address"), py::arg("data"))
      .def(
          "emit_time_code",
          [](spw_rmap::SpwRmapNodeBase& self, std::uint8_t tc) -> void {
            unwrap_or_throw(self.emitTimeCode(tc));
          },
          py::arg("timecode"));

  py::enum_<spw_rmap::BufferPolicy>(m, "SpwRmapBufferPolicy")
      .value("Fixed", spw_rmap::BufferPolicy::Fixed)
      .value("AutoResize", spw_rmap::BufferPolicy::AutoResize)
      .export_values();

  py::class_<spw_rmap::SpwRmapTCPNode, spw_rmap::SpwRmapNodeBase>(
      m, "SpwRmapTCPNode")
      .def(py::init([](const std::string& ip, const std::string& port,
                       size_t send_sz, size_t recv_sz,
                       spw_rmap::BufferPolicy policy)
                        -> std::unique_ptr<spw_rmap::SpwRmapTCPNode> {
             spw_rmap::SpwRmapTCPNodeConfig cfg{.ip_address = ip,
                                                .port = port,
                                                .send_buffer_size = send_sz,
                                                .recv_buffer_size = recv_sz,
                                                .buffer_policy = policy};
             return std::make_unique<spw_rmap::SpwRmapTCPNode>(cfg);
           }),
           py::arg("ip_address"), py::arg("port"),
           py::arg_v("send_buffer_size", 4096),
           py::arg_v("recv_buffer_size", 4096),
           py::arg_v("buffer_policy", spw_rmap::BufferPolicy::AutoResize,
                     "SpwRmapBufferPolicy.AutoResize"))
      .def(
          "connect",
          [](spw_rmap::SpwRmapTCPNode& self,
             std::chrono::microseconds recv_timeout,
             std::chrono::microseconds send_timeout,
             std::chrono::microseconds connect_timeout) -> void {
            unwrap_or_throw(
                self.connect(recv_timeout, send_timeout, connect_timeout));
          },
          py::arg("recv_timeout") = 100ms, py::arg("send_timeout") = 100ms,
          py::arg("connect_timeout") = 100ms)
      .def("run_loop",
           [](spw_rmap::SpwRmapTCPNode& self) -> void {
             std::thread([&self] -> void {
               auto res = self.runLoop();
               if (!res) {
                 py::gil_scoped_acquire gil;
                 throw std::system_error(res.error());
               }
             }).detach();
           })
      .def(
          "write",
          [](spw_rmap::SpwRmapTCPNode& self,
             std::shared_ptr<spw_rmap::TargetNodeBase> target_node,
             std::uint32_t memory_address,
             std::span<const std::uint8_t> data) -> void {
            return unwrap_or_throw(
                self.write(target_node, memory_address, data));
          },
          py::arg("target_node"), py::arg("memory_address"), py::arg("data"))
      .def(
          "read",
          [](spw_rmap::SpwRmapTCPNode& self,
             std::shared_ptr<spw_rmap::TargetNodeBase> target_node,
             std::uint32_t memory_address,
             std::span<std::uint8_t> data) -> void {
            return unwrap_or_throw(
                self.read(target_node, memory_address, data));
          },
          py::arg("target_node"), py::arg("memory_address"), py::arg("data"))
      .def(
          "emit_time_code",
          [](spw_rmap::SpwRmapTCPNode& self, std::uint8_t tc) -> void {
            unwrap_or_throw(self.emitTimeCode(tc));
          },
          py::arg("timecode"));
}
