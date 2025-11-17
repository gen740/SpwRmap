#include <spw_rmap/internal/thread_pool.hh>

namespace spw_rmap::internal {

auto runtime_semaphore::acquire() noexcept -> void {
  std::unique_lock<std::mutex> lock(mtx_);
  cv_.wait(lock, [this]() -> bool { return count_ > 0; });
  --count_;
}

void runtime_semaphore::release() noexcept {
  std::lock_guard<std::mutex> lock(mtx_);
  ++count_;
  cv_.notify_one();
}

Worker::Worker(runtime_semaphore& semaphore) : semaphore_(semaphore) {
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
}

Worker::~Worker() {
  stop();
  if (thread_.joinable()) {
    thread_.join();
  }
}

auto Worker::stop() noexcept -> void {
  std::lock_guard<std::mutex> lock(mtx_);
  stop_ = true;
  cv_.notify_one();
}

auto Worker::isBusy() const noexcept -> bool {
  std::lock_guard<std::mutex> lock(mtx_);
  return running_ || current_task_ != nullptr;
}

auto Worker::post(std::function<void()> func) noexcept
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

ThreadPool::ThreadPool(int num_threads) : semaphore_(num_threads) {
  for (int i = 0; i < num_threads; ++i) {
    workers_.emplace_back(std::make_unique<Worker>(semaphore_));
  }
}

auto ThreadPool::post(std::function<void()> func) noexcept
    -> std::expected<std::monostate, std::error_code> {
  semaphore_.acquire();
  std::lock_guard<std::mutex> lock(mtx_);
  for (auto& worker : workers_) {
    if (!worker->isBusy()) {
      return worker->post(std::move(func));
    }
  }
  assert(false && "Semaphore and worker state are inconsistent");
  return std::unexpected{
      std::make_error_code(std::errc::resource_unavailable_try_again)};
}

auto ThreadPool::stop() noexcept -> void {
  std::lock_guard<std::mutex> lock(mtx_);
  for (auto& worker : workers_) {
    worker->stop();
  }
}

}  // namespace spw_rmap::internal
