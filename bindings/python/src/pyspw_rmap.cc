#include <pybind11/chrono.h>
#include <pybind11/stl.h>

#include <atomic>
#include <chrono>
#include <exception>
#include <iostream>
#include <mutex>
#include <span>
#include <spw_rmap/spw_rmap_node_base.hh>
#include <spw_rmap/spw_rmap_tcp_node.hh>
#include <spw_rmap/target_node.hh>
#include <stdexcept>
#include <thread>

#include "span_caster.hh"

namespace py = pybind11;

using namespace std::chrono_literals;

struct PyTargetNode {
  uint32_t logical_address{0};
  std::vector<uint8_t> target_spacewire_address{};
  std::vector<uint8_t> reply_address{};
};

class PySpwRmapTCPNode {
 public:
  PySpwRmapTCPNode(const PySpwRmapTCPNode&) = delete;
  PySpwRmapTCPNode(PySpwRmapTCPNode&&) = delete;
  auto operator=(const PySpwRmapTCPNode&) -> PySpwRmapTCPNode& = delete;
  auto operator=(PySpwRmapTCPNode&&) -> PySpwRmapTCPNode& = delete;

  PySpwRmapTCPNode(std::string ip_address, std::string port)
      : node_({.ip_address = ip_address,
               .port = port,
               .send_buffer_size = 4096,
               .recv_buffer_size = 4096,
               .buffer_policy = spw_rmap::BufferPolicy::AutoResize}) {}

  ~PySpwRmapTCPNode() {
    try {
      stop();
    } catch (const std::exception& e) {
      std::cerr << "PySpwRmapTCPNode::~PySpwRmapTCPNode: " << e.what() << "\n";
    }
    joinThread_();
  }

  void start() {
    if (running_.load()) {
      throw std::runtime_error("SpwRmapTCPNode already running");
    }
    if (thread_.joinable()) {
      joinThread_();
      rethrowThreadError_();
    }

    auto res = node_.connect(5000ms);
    if (!res) {
      throw std::system_error(res.error());
    }
    connected_.store(true);
    running_.store(true);
    {
      std::lock_guard<std::mutex> lock(thread_error_mtx_);
      thread_error_ = nullptr;
    }
    thread_ = std::thread([this]() -> void {
      try {
        auto run_res = node_.runLoop();
        if (!run_res) {
          throw std::system_error(run_res.error());
        }
      } catch (...) {
        std::lock_guard<std::mutex> lock(thread_error_mtx_);
        thread_error_ = std::current_exception();
      }
      running_.store(false);
      connected_.store(false);
    });
  }

  void stop() {
    if (!running_.load() && !connected_.load()) {
      joinThread_();
      rethrowThreadError_();
      return;
    }
    if (connected_.load()) {
      auto res = node_.shutdown();
      connected_.store(false);
      running_.store(false);
      if (!res) {
        joinThread_();
        rethrowThreadError_();
        throw std::system_error(res.error());
      }
    } else {
      running_.store(false);
    }
    joinThread_();
    rethrowThreadError_();
  }

  auto read(PyTargetNode target_node, uint32_t memory_adderss,
            uint32_t data_length) -> std::vector<uint8_t> {
    checkThreadError_();
    std::vector<uint8_t> data(data_length);
    auto target_node_ptr = std::make_shared<spw_rmap::TargetNodeDynamic>(
        static_cast<uint8_t>(target_node.logical_address),
        std::move(target_node.target_spacewire_address),
        std::move(target_node.reply_address));
    auto future = node_.readAsync(
        target_node_ptr, memory_adderss, data.size(),
        [&data](const spw_rmap::Packet& packet) noexcept -> void {
          std::copy_n(packet.data.data(), data.size(), data.data());
        });

    if (future.wait_for(std::chrono::seconds(1)) ==
        std::future_status::timeout) {
      throw std::system_error(std::make_error_code(std::errc::timed_out));
    } else {
      auto res = future.get();
      if (!res) {
        throw std::system_error(res.error());
      }
    }
    return data;
  }

  void write(PyTargetNode target_node, uint32_t memory_adderss,
             const std::vector<uint8_t>& data) {
    checkThreadError_();
    auto target_node_ptr = std::make_shared<spw_rmap::TargetNodeDynamic>(
        static_cast<uint8_t>(target_node.logical_address),
        std::move(target_node.target_spacewire_address),
        std::move(target_node.reply_address));
    auto future =
        node_.writeAsync(target_node_ptr, memory_adderss,
                         std::span<const uint8_t>(data.data(), data.size()),
                         [](const spw_rmap::Packet&) noexcept -> void {});
    if (future.wait_for(std::chrono::seconds(1)) ==
        std::future_status::timeout) {
      throw std::system_error(std::make_error_code(std::errc::timed_out));
    } else {
      auto res = future.get();
      if (!res) {
        throw std::system_error(res.error());
      }
    }
  }

 private:
  void joinThread_() noexcept {
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  void rethrowThreadError_() {
    std::exception_ptr err;
    {
      std::lock_guard<std::mutex> lock(thread_error_mtx_);
      err = thread_error_;
      thread_error_ = nullptr;
    }
    if (err) {
      std::rethrow_exception(err);
    }
  }

  void checkThreadError_() {
    std::exception_ptr err;
    {
      std::lock_guard<std::mutex> lock(thread_error_mtx_);
      err = thread_error_;
    }
    if (err) {
      std::rethrow_exception(err);
    }
  }

  spw_rmap::SpwRmapTCPClient node_;
  std::thread thread_;
  std::mutex thread_error_mtx_;
  std::exception_ptr thread_error_ = nullptr;
  std::atomic<bool> running_{false};
  std::atomic<bool> connected_{false};
};

PYBIND11_MODULE(_core, m) {
  py::class_<PyTargetNode>(m, "TargetNode")
      .def(py::init<>())
      .def_readwrite("logical_address", &PyTargetNode::logical_address)
      .def_readwrite("target_spacewire_address",
                     &PyTargetNode::target_spacewire_address)
      .def_readwrite("reply_address", &PyTargetNode::reply_address);

  py::class_<PySpwRmapTCPNode>(m, "SpwRmapTCPNode")
      .def(py::init<std::string, std::string>(), py::arg("ip_address"),
           py::arg("port"))
      .def("start", &PySpwRmapTCPNode::start)
      .def("stop", &PySpwRmapTCPNode::stop)
      .def("read", &PySpwRmapTCPNode::read, py::arg("target_node"),
           py::arg("memory_address"), py::arg("data_length"))
      .def("write", &PySpwRmapTCPNode::write, py::arg("target_node"),
           py::arg("memory_address"), py::arg("data"));
}
