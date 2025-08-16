#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <SpwRmap/LegacySpwRmapTCPNode.hh>
#include <SpwRmap/SpwRmapNodeBase.hh>
#include <SpwRmap/SpwRmapTCPNode.hh>
#include <SpwRmap/TargetNode.hh>

#include "span_caster.hh"

namespace py = pybind11;

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

  [[nodiscard]] auto write(const SpwRmap::TargetNodeBase &, uint32_t,
                           const std::span<const uint8_t>) noexcept
      -> std::expected<std::monostate, std::error_code> override {
    using R = std::expected<std::monostate, std::error_code>;
    PYBIND11_OVERRIDE_PURE(R, SpwRmap::SpwRmapNodeBase, write);
  }

  [[nodiscard]] auto read(const SpwRmap::TargetNodeBase &, uint32_t,
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
          [](SpwRmap::SpwRmapNodeBase &self,
             const SpwRmap::TargetNodeBase &target_node,
             std::uint32_t memory_address, std::span<const std::uint8_t> data) {
            return unwrap_or_throw(
                self.write(target_node, memory_address, data));
          },
          py::arg("target_node"), py::arg("memory_address"), py::arg("data"))
      .def(
          "read",
          [](SpwRmap::SpwRmapNodeBase &self,
             const SpwRmap::TargetNodeBase &target_node,
             std::uint32_t memory_address, std::span<std::uint8_t> data) {
            return unwrap_or_throw(
                self.read(target_node, memory_address, data));
          },
          py::arg("target_node"), py::arg("memory_address"), py::arg("data"))
      .def(
          "emit_time_code",
          [](SpwRmap::SpwRmapNodeBase &self, std::uint8_t tc) -> void {
            unwrap_or_throw(self.emitTimeCode(tc));
          },
          py::arg("timecode"));

  py::class_<SpwRmap::LegacySpwRmapTCPNode, SpwRmap::SpwRmapNodeBase>(
      m, "LegacySpwRmapTCPNode")
      .def(py::init<std::string_view, uint32_t>(), py::arg("ip_address"),
           py::arg("port"))
      .def(
          "write",
          [](SpwRmap::LegacySpwRmapTCPNode &self,
             const SpwRmap::TargetNodeBase &target_node,
             std::uint32_t memory_address, std::span<const std::uint8_t> data) {
            return unwrap_or_throw(
                self.write(target_node, memory_address, data));
          },
          py::arg("target_node"), py::arg("memory_address"), py::arg("data"))
      .def(
          "read",
          [](SpwRmap::LegacySpwRmapTCPNode &self,
             const SpwRmap::TargetNodeBase &target_node,
             std::uint32_t memory_address, std::span<std::uint8_t> data) {
            return unwrap_or_throw(
                self.read(target_node, memory_address, data));
          },
          py::arg("target_node"), py::arg("memory_address"), py::arg("data"))
      .def(
          "emit_time_code",
          [](SpwRmap::LegacySpwRmapTCPNode &self, std::uint8_t tc) -> void {
            unwrap_or_throw(self.emitTimeCode(tc));
          },
          py::arg("timecode"));

  py::class_<SpwRmap::SpwRmapTCPNode, SpwRmap::SpwRmapNodeBase>(
      m, "SpwRmapTCPNode")
      .def(py::init<std::string_view, uint32_t>(), py::arg("ip_address"),
           py::arg("port"))
      .def("connect", &SpwRmap::SpwRmapTCPNode::connect)
      .def(
          "set_buffer",
          [](SpwRmap::SpwRmapTCPNode &self, size_t send_buf_size,
             size_t recv_buf_size) {
            return self.setBuffer(send_buf_size, recv_buf_size);
          },
          py::arg("send_buffer_size"), py::arg("recv_buffer_size"))
      .def(
          "write",
          [](SpwRmap::SpwRmapTCPNode &self,
             const SpwRmap::TargetNodeBase &target_node,
             std::uint32_t memory_address, std::span<const std::uint8_t> data) {
            return unwrap_or_throw(
                self.write(target_node, memory_address, data));
          },
          py::arg("target_node"), py::arg("memory_address"), py::arg("data"))
      .def(
          "read",
          [](SpwRmap::SpwRmapTCPNode &self,
             const SpwRmap::TargetNodeBase &target_node,
             std::uint32_t memory_address, std::span<std::uint8_t> data) {
            return unwrap_or_throw(
                self.read(target_node, memory_address, data));
          },
          py::arg("target_node"), py::arg("memory_address"), py::arg("data"))
      .def(
          "emit_time_code",
          [](SpwRmap::SpwRmapTCPNode &self, std::uint8_t tc) -> void {
            unwrap_or_throw(self.emitTimeCode(tc));
          },
          py::arg("timecode"));
}
