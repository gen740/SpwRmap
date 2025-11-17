#pragma once

#include <cassert>
#include <condition_variable>
#include <expected>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

namespace spw_rmap::internal {

/**
 * @class runtime_semaphore
 * @brief A simple counting semaphore implementation for runtime use.
 *
 * This class provides a basic counting semaphore mechanism to control access
 * to shared resources in a multithreaded environment.
 */
class runtime_semaphore {
 public:
  explicit runtime_semaphore(int initial) : count_(initial) {}
  void acquire() noexcept;
  void release() noexcept;

 private:
  int count_;
  std::mutex mtx_;
  std::condition_variable cv_;
};

class Worker {
 public:
  Worker(runtime_semaphore& semaphore);

  ~Worker();

  Worker(const Worker&) = delete;
  auto operator=(const Worker&) -> Worker& = delete;
  Worker(Worker&&) = delete;
  auto operator=(Worker&&) -> Worker& = delete;

  auto stop() noexcept -> void;

  auto isBusy() const noexcept -> bool;

  auto post(std::function<void()> func) noexcept
      -> std::expected<std::monostate, std::error_code>;

 private:
  bool stop_{false};
  bool running_{false};
  std::thread thread_;
  std::function<void()> current_task_;
  std::condition_variable cv_;
  mutable std::mutex mtx_;
  runtime_semaphore& semaphore_;
};

/**
 * @class ThreadPool
 * @brief A simple thread pool implementation.
 *
 * This class manages a pool of worker threads to execute tasks concurrently.
 * There is no task queue; if all workers are busy, posting a new task will wait
 */
class ThreadPool {
  std::vector<std::unique_ptr<Worker>> workers_ = {};

 public:
  ThreadPool(int num_threads);
  ~ThreadPool() { stop(); }

  ThreadPool(const ThreadPool&) = delete;
  auto operator=(const ThreadPool&) -> ThreadPool& = delete;
  ThreadPool(ThreadPool&&) = delete;
  auto operator=(ThreadPool&&) -> ThreadPool& = delete;

  auto post(std::function<void()> func) noexcept
      -> std::expected<std::monostate, std::error_code>;

  auto stop() noexcept -> void;

 private:
  std::mutex mtx_;
  runtime_semaphore semaphore_;
};

}  // namespace spw_rmap::internal
