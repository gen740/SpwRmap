#pragma once

#include <cassert>
#include <condition_variable>
#include <expected>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

namespace spw_rmap::internal {

class runtime_semaphore {
 public:
  explicit runtime_semaphore(int initial) : count_(initial) {}

  void acquire() {
    std::unique_lock<std::mutex> lock(mtx_);
    cv_.wait(lock, [this]() -> bool { return count_ > 0; });
    --count_;
  }

  void release() {
    std::lock_guard<std::mutex> lock(mtx_);
    ++count_;
    cv_.notify_one();
  }

 private:
  int count_;
  std::mutex mtx_;
  std::condition_variable cv_;
};

class Worker {
 public:
  Worker(runtime_semaphore& semaphore) : semaphore_(semaphore) {
    thread_ = std::thread([this]() -> void {
      while (true) {
        std::function<void()> task;
        {
          std::unique_lock<std::mutex> lock(mtx_);
          cv_.wait(lock, [this]() -> bool {
            return stop_ || current_task_ != nullptr;
          });
          if (stop_) {
            break;
          }
          running_ = true;
          task = std::move(current_task_);
          current_task_ = nullptr;
        }
        task();
        {
          std::lock_guard<std::mutex> lock(mtx_);
          running_ = false;
          semaphore_.release();
        }
      }
    });
  };

  ~Worker() {
    stop();
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  Worker(const Worker&) = delete;
  auto operator=(const Worker&) -> Worker& = delete;
  Worker(Worker&&) = delete;
  auto operator=(Worker&&) -> Worker& = delete;

  auto stop() noexcept -> void {
    std::lock_guard<std::mutex> lock(mtx_);
    stop_ = true;
    cv_.notify_one();
  }

  auto isBusy() const -> bool {
    std::lock_guard<std::mutex> lock(mtx_);
    return running_ || current_task_ != nullptr;
  }

  auto post(std::function<void()> func) noexcept
      -> std::expected<std::monostate, std::error_code> {
    std::lock_guard<std::mutex> lock(mtx_);
    if (stop_) {
      return std::unexpected{
          std::make_error_code(std::errc::operation_not_permitted)};
    }
    current_task_ = std::move(func);
    cv_.notify_one();
    return {};
  }

 private:
  bool stop_{false};
  bool running_{false};
  std::thread thread_;
  std::function<void()> current_task_;
  std::condition_variable cv_;
  mutable std::mutex mtx_;
  runtime_semaphore& semaphore_;
};

class ThreadPool {
  std::vector<std::unique_ptr<Worker>> workers_ = {};

 public:
  ThreadPool(int num_threads) : semaphore_(num_threads) {
    for (int i = 0; i < num_threads; ++i) {
      workers_.emplace_back(std::make_unique<Worker>(semaphore_));
    }
  }
  ~ThreadPool() { stop(); }

  ThreadPool(const ThreadPool&) = delete;
  auto operator=(const ThreadPool&) -> ThreadPool& = delete;
  ThreadPool(ThreadPool&&) = delete;
  auto operator=(ThreadPool&&) -> ThreadPool& = delete;

  auto post(std::function<void()> func) noexcept
      -> std::expected<std::monostate, std::error_code> {
    semaphore_.acquire();
    std::lock_guard<std::mutex> lock(mtx_);
    for (auto& worker : workers_) {
      if (!worker->isBusy()) {
        return worker->post(std::move(func));
      }
    }
    assert(false && "Semaphore and worker state are inconsistent");
  }

  auto stop() noexcept -> void {
    std::lock_guard<std::mutex> lock(mtx_);
    for (auto& worker : workers_) {
      worker->stop();
    }
  }

 private:
  std::mutex mtx_;
  runtime_semaphore semaphore_;
};

}  // namespace spw_rmap::internal
