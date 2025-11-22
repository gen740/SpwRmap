#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <spw_rmap/spw_rmap_tcp_node.hh>
#include <string>
#include <thread>
#include <vector>

#include "spw_rmap/packet_parser.hh"
#include "spw_rmap/target_node.hh"

using namespace std::chrono_literals;

namespace {

using Clock = std::chrono::steady_clock;
using Microseconds = std::chrono::microseconds;

auto parse_int(const char* s) -> std::optional<long long> {
  char* end = nullptr;
  errno = 0;
  long long v = std::strtoll(s, &end, 10);
  if (errno != 0 || end == s || *end != '\0') {
    return std::nullopt;
  }
  return v;
}

auto compute_mean(const std::vector<double>& xs) -> double {
  if (xs.empty()) {
    return 0.0;
  }
  double sum = std::accumulate(xs.begin(), xs.end(), 0.0);
  return sum / static_cast<double>(xs.size());
}

auto compute_stddev(const std::vector<double>& xs, double mean) -> double {
  if (xs.size() <= 1) {
    return 0.0;
  }
  double acc = 0.0;
  for (double x : xs) {
    double d = x - mean;
    acc += d * d;
  }
  return std::sqrt(acc / static_cast<double>(xs.size()));
}

auto compute_min(const std::vector<double>& xs) -> double {
  if (xs.empty()) {
    return 0.0;
  }
  return *std::ranges::min_element(xs);
}

auto compute_max(const std::vector<double>& xs) -> double {
  if (xs.empty()) {
    return 0.0;
  }
  return *std::ranges::max_element(xs);
}

static auto median_sorted(const std::vector<double>& xs_sorted) -> double {
  const size_t n = xs_sorted.size();
  if (n == 0) {
    return 0.0;
  }
  if (n % 2 == 1) {
    return xs_sorted[n / 2];
  } else {
    return 0.5 * (xs_sorted[n / 2 - 1] + xs_sorted[n / 2]);
  }
}

auto compute_median(const std::vector<double>& xs) -> double {
  if (xs.empty()) {
    return 0.0;
  }
  std::vector<double> v(xs);
  std::ranges::sort(v);
  return median_sorted(v);
}

auto compute_q1(const std::vector<double>& xs) -> double {
  if (xs.size() < 2) {
    return compute_median(xs);
  }
  std::vector<double> v(xs);
  std::ranges::sort(v);

  const size_t n = v.size();
  size_t mid = n / 2;

  std::vector<double> lower(v.begin(), v.begin() + mid);  // NOLINT
  return median_sorted(lower);
}

auto compute_q3(const std::vector<double>& xs) -> double {
  if (xs.size() < 2) {
    return compute_median(xs);
  }
  std::vector<double> v(xs);
  std::ranges::sort(v);

  const size_t n = v.size();
  size_t mid = n / 2;

  std::vector<double> upper;
  if (n % 2 == 1) {
    upper.assign(v.begin() + mid + 1, v.end());  // NOLINT
  } else {
    upper.assign(v.begin() + mid, v.end());  // NOLINT
  }

  return median_sorted(upper);
}

}  // namespace

auto main(int argc, char** argv) -> int {
  if (argc != 4) {
    std::cerr << "Usage: " << argv[0] << " ntimes nbytes nbatch_read\n"
              << "  ntimes: number of repetitions\n"
              << "  nbytes: total bytes per read\n"
              << "  nbatch_read: number of batches per read\n";
    return 1;
  }

  auto ntimes_opt = parse_int(argv[1]);
  auto nbytes_opt = parse_int(argv[2]);
  auto nbatch_read_opt = parse_int(argv[3]);

  if (!ntimes_opt || !nbytes_opt || !nbatch_read_opt) {
    std::cerr << "Error: arguments must be positive integers.\n";
    return 1;
  }

  const auto ntimes_ll = *ntimes_opt;
  const auto nbytes_ll = *nbytes_opt;
  const auto nbatch_read_ll = *nbatch_read_opt;

  if (ntimes_ll <= 0 || nbytes_ll <= 0 || nbatch_read_ll <= 0) {
    std::cerr << "Error: arguments must be > 0.\n";
    return 1;
  }

  const auto ntimes = static_cast<std::size_t>(ntimes_ll);
  const auto nbytes = static_cast<std::size_t>(nbytes_ll);
  const auto nbatch_read = static_cast<std::size_t>(nbatch_read_ll);

  if (nbytes % nbatch_read != 0) {
    std::cerr << "Error: nbytes % nbatch must be 0.\n";
    return 1;
  }

  const std::size_t chunk_size_read = nbytes / nbatch_read;
  const std::uint32_t base_address = 0x00000000u;

  std::cout << "ntimes = " << ntimes << ", nbytes = " << nbytes
            << ", nbatch_read = " << nbatch_read << "\n"
            << "  chunk_size_read = " << chunk_size_read << " bytes\n";

  const std::string data_path = "workdir/data.bin";
  std::vector<std::uint8_t> expected_data;
  {
    std::ifstream data_ifs(data_path, std::ios::binary);
    if (!data_ifs) {
      std::cerr << "Error: failed to open " << data_path << " for reading.\n";
      return 1;
    }
    data_ifs.seekg(0, std::ios::end);
    const auto file_size = data_ifs.tellg();
    if (file_size < 0) {
      std::cerr << "Error: failed to determine size of " << data_path << ".\n";
      return 1;
    }
    data_ifs.seekg(0, std::ios::beg);
    expected_data.resize(static_cast<std::size_t>(file_size));
    if (!expected_data.empty()) {
      if (!data_ifs.read(
              reinterpret_cast<char*>(expected_data.data()),  // NOLINT
              static_cast<std::streamsize>(expected_data.size()))) {
        std::cerr << "Error: failed to read " << data_path << ".\n";
        return 1;
      }
    }
  }

  ////////////////////////////////////////////////////////////////////////////////

  // Setup SpaceWire RMAP TCP client
  auto spw = spw_rmap::SpwRmapTCPClient({
      .ip_address = "192.168.1.100",
      .port = "10030",
      .send_buffer_size = 9000,
      .recv_buffer_size = 9000,
  });
  spw.setVerifyMode(true);
  spw.setInitiatorLogicalAddress(0xFE);

  auto res_con = spw.connect(1s);

  if (res_con.has_value()) {
    std::cout << "Connected to SpaceWire RMAP TCP Node." << std::endl;
  } else {
    std::cerr << "Connection error: " << res_con.error().message() << std::endl;
    return 1;
  }

  auto target1 = std::make_shared<spw_rmap::TargetNodeDynamic>(
      0x32, std::vector<uint8_t>{6, 2}, std::vector<uint8_t>{1, 3});
  auto target2 = std::make_shared<spw_rmap::TargetNodeDynamic>(
      0x33, std::vector<uint8_t>{7, 2}, std::vector<uint8_t>{1, 3});

  // Run loop thread
  std::thread loop_thread([&spw]() -> void {
    auto res = spw.runLoop();
    if (!res.has_value()) {
      std::cerr << "runLoop error: " << res.error().message() << std::endl;
    }
  });

  std::vector<double> read_times_us;
  read_times_us.reserve(ntimes);

  // Measurement loop
  for (std::size_t iter = 0; iter < ntimes; ++iter) {
    std::cout << "Iteration " << (iter + 1) << " / " << ntimes << std::endl;

    std::vector<std::uint8_t> read_buf1(nbytes, 0);
    std::vector<std::uint8_t> read_buf2(nbytes, 0);
    // ----------------------------
    // Read measurement
    // ----------------------------
    std::atomic<std::size_t> read_cb_count1{0};
    std::atomic<std::size_t> read_cb_count2{0};
    bool read_done = false;
    Clock::time_point read_start;
    Clock::time_point read_end;
    std::mutex read_mtx;

    std::vector<std::future<std::expected<std::monostate, std::error_code>>>
        read_futures1;
    std::vector<std::future<std::expected<std::monostate, std::error_code>>>
        read_futures2;
    read_futures1.reserve(nbatch_read);
    read_futures2.reserve(nbatch_read);

    read_start = Clock::now();
    for (std::size_t i = 0; i < nbatch_read; ++i) {
      std::size_t offset = i * chunk_size_read;
      std::uint32_t addr = base_address + static_cast<std::uint32_t>(offset);
      auto fut1 = spw.readAsync(
          target1, addr, chunk_size_read,
          [&read_buf1, offset, chunk_size_read, &read_cb_count1, &read_mtx,
           &read_end, &read_done,
           total = nbatch_read](spw_rmap::Packet packet) -> void {
            auto data_size = packet.data.size();
            if (data_size != chunk_size_read) {
              throw std::runtime_error(
                  "Read callback: unexpected data size received.");
            }
            std::ranges::copy(packet.data, read_buf1.begin() +
                                               static_cast<ptrdiff_t>(offset));
            auto count =
                read_cb_count1.fetch_add(1, std::memory_order_acq_rel) + 1;
            if (count == total) {
              auto now = Clock::now();
              {
                std::lock_guard<std::mutex> lk(read_mtx);
                read_end = now;
                read_done = true;
              }
            }
          });
      read_futures1.emplace_back(std::move(fut1));

      auto fut2 = spw.readAsync(
          target2, addr, chunk_size_read,
          [&read_buf2, offset, chunk_size_read, &read_cb_count2, &read_mtx,
           &read_end, &read_done,
           total = nbatch_read](spw_rmap::Packet packet) -> void {
            auto data_size = packet.data.size();
            if (data_size != chunk_size_read) {
              throw std::runtime_error(
                  "Read callback: unexpected data size received.");
            }
            std::ranges::copy(packet.data, read_buf2.begin() +
                                               static_cast<ptrdiff_t>(offset));
            auto count =
                read_cb_count2.fetch_add(1, std::memory_order_acq_rel) + 1;
            if (count == total) {
              auto now = Clock::now();
              {
                std::lock_guard<std::mutex> lk(read_mtx);
                read_end = now;
                read_done = true;
              }
            }
          });
      read_futures2.emplace_back(std::move(fut2));
    }

    // while (true) {
    //   size_t wait_counter = 0;
    //   for (auto& fut : read_futures1) {
    //     if (fut.wait_for(0ms) == std::future_status::timeout) {
    //       wait_counter++;
    //     }
    //   }
    //   std::cout << "Read: waiting for " << wait_counter << " futures..."
    //             << std::endl;
    //   if (wait_counter == 1) {
    //     break;
    //   }
    //   std::this_thread::sleep_for(10us);
    // }

    for (auto& fut : read_futures1) {
      fut.wait();
      auto res = fut.get();
      if (!res.has_value()) {
        std::cerr << "Read error during iteration " << (iter + 1) << ": "
                  << res.error().message() << std::endl;
        goto shutdown;
      }
    }

    for (auto& fut : read_futures2) {
      fut.wait();
      auto res = fut.get();
      if (!res.has_value()) {
        std::cerr << "Read error during iteration " << (iter + 1) << ": "
                  << res.error().message() << std::endl;
        goto shutdown;
      }
    }

    {
      auto dt = std::chrono::duration_cast<Microseconds>(read_end - read_start);
      read_times_us.push_back(static_cast<double>(dt.count()));
    }

    // ----------------------------
    // Verify data
    // ----------------------------
    if (!std::equal(expected_data.begin(),
                    expected_data.begin() +
                        static_cast<std::ptrdiff_t>(read_buf1.size()),
                    read_buf1.begin())) {
      for (std::size_t i = 0; i < read_buf1.size(); ++i) {
        if (expected_data[i] != read_buf1[i]) {
          std::cerr << "  Mismatch at byte offset " << i << " (address 0x"
                    << std::hex << (i) << std::dec << "): expected "
                    << static_cast<int>(expected_data[i]) << ", read "
                    << static_cast<int>(read_buf1[i]) << "\n";
          break;
        }
      }

      std::cerr << "Error: data mismatch detected in iteration " << (iter + 1)
                << ".\n";
      goto shutdown;
    }

    if (!std::equal(expected_data.begin(),
                    expected_data.begin() +
                        static_cast<std::ptrdiff_t>(read_buf2.size()),
                    read_buf2.begin())) {
      for (std::size_t i = 0; i < read_buf2.size(); ++i) {
        if (expected_data[i] != read_buf2[i]) {
          std::cerr << "  Mismatch at byte offset " << i << " (address 0x"
                    << std::hex << (i) << std::dec << "): expected "
                    << static_cast<int>(expected_data[i]) << ", read "
                    << static_cast<int>(read_buf2[i]) << "\n";
          break;
        }
      }

      std::cerr << "Error: data mismatch detected in iteration " << (iter + 1)
                << ".\n";
      goto shutdown;
    }

    std::cout << "\rIteration " << (iter + 1) << " / " << ntimes << " OK."
              << std::flush;
    std::this_thread::sleep_for(10ms);
  }

  {
    double r_mean = compute_mean(read_times_us);
    double r_std = compute_stddev(read_times_us, r_mean);

    double r_min = compute_min(read_times_us);
    double r_q1 = compute_q1(read_times_us);
    double r_median = compute_median(read_times_us);
    double r_q3 = compute_q3(read_times_us);
    double r_max = compute_max(read_times_us);

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Read time (microseconds):\n"
              << "  mean = " << r_mean << " us\n"
              << "  std  = " << r_std << " us\n";
    std::ofstream ofs("spwrmap_speedtest_results.txt", std::ios::app);
    if (ofs) {
      ofs << nbytes << ": "
          << " (" << r_mean << ", " << r_std << ", " << r_min << ", " << r_q1
          << ", " << r_median << ", " << r_q3 << ", " << r_max << "),\n";
      ofs.close();
    }
  }

shutdown: {
  auto res = spw.shutdown();
  if (!res.has_value()) {
    std::cerr << "Shutdown error: " << res.error().message() << std::endl;
  } else {
    std::cout << "Shutdown successfully." << std::endl;
  }
}

  if (loop_thread.joinable()) {
    loop_thread.join();
  }

  return 0;
}
