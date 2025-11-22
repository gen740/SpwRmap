#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include "spw_rmap/spw_rmap_tcp_node.hh"
#include "spw_rmap/target_node.hh"

using namespace std::chrono_literals;

namespace {

constexpr uint8_t kInitiatorLogicalAddress = 0xFE;
constexpr uint8_t kTargetLogicalAddress = 0xFE;
constexpr std::size_t kChunkSize = 1024;

using Clock = std::chrono::steady_clock;

struct Options {
  std::string ip{"127.0.0.1"};
  std::string port{"10030"};
  std::vector<uint8_t> target_address;
  std::vector<uint8_t> reply_address;
  std::optional<std::size_t> ntimes;
  std::optional<std::size_t> nbytes;
  std::optional<uint32_t> start_address;
};

void printUsage(const char* program) {
  std::cerr << "Usage: " << program << '\n'
            << "  --ip <addr> --port <port> --target-address <bytes...>\n"
            << "  --reply-address <bytes...> --ntimes <count> --nbytes <size>\n"
            << "  --start_address <addr>\n";
}

auto parseUnsigned(std::string_view token, unsigned long long max_value)
    -> std::optional<unsigned long long> {
  try {
    std::string temp(token);
    size_t idx = 0;
    auto value = std::stoull(temp, &idx, 0);
    if (idx != temp.size() || value > max_value) {
      return std::nullopt;
    }
    return value;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

auto parseByteSequence(int argc, char** argv, int& index,
                       std::vector<uint8_t>& dst, std::string_view option)
    -> bool {
  bool parsed = false;
  while (index + 1 < argc) {
    std::string_view next = argv[index + 1];
    if (next.starts_with("--")) {
      break;
    }
    ++index;
    auto value = parseUnsigned(next, 0xFF);
    if (!value.has_value()) {
      std::cerr << "Invalid value for --" << option << ": '" << next << "'\n";
      return false;
    }
    dst.push_back(static_cast<uint8_t>(*value));
    parsed = true;
  }
  if (!parsed) {
    std::cerr << "--" << option << " requires at least one byte.\n";
  }
  return parsed;
}

auto parseOptions(int argc, char** argv) -> std::optional<Options> {
  Options opts{};

  for (int i = 1; i < argc; ++i) {
    std::string_view arg = argv[i];
    if (!arg.starts_with("--")) {
      std::cerr << "Unknown argument: " << arg << "\n";
      return std::nullopt;
    }
    auto name = arg.substr(2);

    auto takeValue = [&](std::string_view opt) -> std::optional<std::string> {
      if (i + 1 >= argc) {
        std::cerr << "--" << opt << " requires a value.\n";
        return std::nullopt;
      }
      return std::string(argv[++i]);
    };

    if (name == "ip") {
      if (auto v = takeValue(name)) {
        opts.ip = std::move(*v);
      } else {
        return std::nullopt;
      }
    } else if (name == "port") {
      if (auto v = takeValue(name)) {
        opts.port = std::move(*v);
      } else {
        return std::nullopt;
      }
    } else if (name == "target-address") {
      if (!parseByteSequence(argc, argv, i, opts.target_address, name)) {
        return std::nullopt;
      }
    } else if (name == "reply-address") {
      if (!parseByteSequence(argc, argv, i, opts.reply_address, name)) {
        return std::nullopt;
      }
    } else if (name == "ntimes") {
      if (auto v = takeValue(name)) {
        auto parsed =
            parseUnsigned(*v, std::numeric_limits<std::size_t>::max());
        if (!parsed.has_value() || *parsed == 0) {
          std::cerr << "Invalid --ntimes: '" << *v << "'\n";
          return std::nullopt;
        }
        opts.ntimes = static_cast<std::size_t>(*parsed);
      } else {
        return std::nullopt;
      }
    } else if (name == "nbytes") {
      if (auto v = takeValue(name)) {
        auto parsed =
            parseUnsigned(*v, std::numeric_limits<std::size_t>::max());
        if (!parsed.has_value() || *parsed == 0 ||
            *parsed > std::numeric_limits<uint32_t>::max()) {
          std::cerr << "--nbytes must be within [1, 0xFFFFFFFF].\n";
          return std::nullopt;
        }
        opts.nbytes = static_cast<std::size_t>(*parsed);
      } else {
        return std::nullopt;
      }
    } else if (name == "start_address") {
      if (auto v = takeValue(name)) {
        auto parsed = parseUnsigned(*v, std::numeric_limits<uint32_t>::max());
        if (!parsed.has_value()) {
          std::cerr << "Invalid --start_address: '" << *v << "'\n";
          return std::nullopt;
        }
        opts.start_address = static_cast<uint32_t>(*parsed);
      } else {
        return std::nullopt;
      }
    } else if (name == "help") {
      printUsage(argv[0]);
      std::exit(0);
    } else {
      std::cerr << "Unknown option: --" << name << "\n";
      return std::nullopt;
    }
  }

  if (opts.target_address.empty()) {
    std::cerr << "--target-address is required.\n";
    return std::nullopt;
  }
  if (opts.reply_address.empty()) {
    std::cerr << "--reply-address is required.\n";
    return std::nullopt;
  }
  if (!opts.ntimes.has_value()) {
    std::cerr << "--ntimes is required.\n";
    return std::nullopt;
  }
  if (!opts.nbytes.has_value()) {
    std::cerr << "--nbytes is required.\n";
    return std::nullopt;
  }
  if (!opts.start_address.has_value()) {
    std::cerr << "--start_address is required.\n";
    return std::nullopt;
  }

  return opts;
}

auto computeMean(const std::vector<double>& xs) -> double {
  if (xs.empty()) {
    return 0.0;
  }
  double sum = std::accumulate(xs.begin(), xs.end(), 0.0);
  return sum / static_cast<double>(xs.size());
}

auto computeStd(const std::vector<double>& xs, double mean) -> double {
  if (xs.size() <= 1) {
    return 0.0;
  }
  double acc = 0.0;
  for (double v : xs) {
    const double diff = v - mean;
    acc += diff * diff;
  }
  return std::sqrt(acc / static_cast<double>(xs.size()));
}

auto medianSorted(const std::vector<double>& xs) -> double {
  if (xs.empty()) {
    return 0.0;
  }
  const std::size_t n = xs.size();
  if (n % 2 == 1) {
    return xs[n / 2];
  }
  return 0.5 * (xs[n / 2 - 1] + xs[n / 2]);
}

auto computeQuartiles(std::vector<double> xs)
    -> std::tuple<double, double, double, double, double> {
  if (xs.empty()) {
    return {0.0, 0.0, 0.0, 0.0, 0.0};
  }
  std::ranges::sort(xs);
  const std::size_t n = xs.size();
  const double min_v = xs.front();
  const double max_v = xs.back();
  const double median = medianSorted(xs);

  const std::size_t mid = n / 2;
  std::vector<double> lower(xs.begin(), xs.begin() + mid);
  std::vector<double> upper;
  if (n % 2 == 0) {
    upper.assign(xs.begin() + mid, xs.end());
  } else {
    upper.assign(xs.begin() + mid + 1, xs.end());
  }
  const double q1 = lower.empty() ? min_v : medianSorted(lower);
  const double q3 = upper.empty() ? max_v : medianSorted(upper);
  return {min_v, q1, median, q3, max_v};
}

void updateProgress(std::size_t current, std::size_t total) {
  static constexpr std::size_t kBarWidth = 40;
  double ratio = total == 0 ? 0.0 : static_cast<double>(current) / total;
  ratio = std::clamp(ratio, 0.0, 1.0);
  auto filled = static_cast<std::size_t>(ratio * kBarWidth);
  std::cerr << '\r' << "[";
  for (std::size_t i = 0; i < kBarWidth; ++i) {
    std::cerr << (i < filled ? '#' : '.');
  }
  std::cerr << "] " << current << "/" << total << std::flush;
}

auto toMicroseconds(double value) -> long long {
  return static_cast<long long>(std::llround(value));
}

}  // namespace

auto main(int argc, char** argv) -> int {
  auto options = parseOptions(argc, argv);
  if (!options) {
    printUsage(argv[0]);
    return 1;
  }
  auto opts = std::move(*options);

  const std::size_t ntimes = *opts.ntimes;
  const std::size_t total_bytes = *opts.nbytes;
  const uint32_t base_address = *opts.start_address;
  const auto range_end = static_cast<unsigned long long>(base_address) +
                         static_cast<unsigned long long>(total_bytes);
  if (range_end >
      static_cast<unsigned long long>(std::numeric_limits<uint32_t>::max()) +
          1ULL) {
    std::cerr << "--start_address + --nbytes exceeds 32-bit address space.\n";
    return 1;
  }

  std::vector<uint8_t> pattern(total_bytes);
  std::mt19937 rng(std::random_device{}());
  std::uniform_int_distribution<int> dist(0, 0xFF);
  for (auto& byte : pattern) {
    byte = static_cast<uint8_t>(dist(rng));
  }

  auto client =
      spw_rmap::SpwRmapTCPClient({.ip_address = opts.ip, .port = opts.port});
  client.setInitiatorLogicalAddress(kInitiatorLogicalAddress);

  auto connect_res = client.connect(1s);
  if (!connect_res.has_value()) {
    std::cerr << "Failed to connect: " << connect_res.error().message() << "\n";
    return 1;
  }

  std::thread loop_thread([&client]() {
    auto res = client.runLoop();
    if (!res.has_value()) {
      std::cerr << "runLoop error: " << res.error().message() << "\n";
    }
  });
  auto joinLoop = [&loop_thread]() noexcept {
    if (loop_thread.joinable()) {
      loop_thread.join();
    }
  };

  auto target = std::make_shared<spw_rmap::TargetNodeDynamic>(
      kTargetLogicalAddress, std::move(opts.target_address),
      std::move(opts.reply_address));

  // Initial write of the pattern into the device memory.
  for (std::size_t offset = 0; offset < total_bytes; offset += kChunkSize) {
    const std::size_t chunk = std::min(kChunkSize, total_bytes - offset);
    std::span<const uint8_t> chunk_span(pattern.data() + offset, chunk);
    auto res = client.write(
        target, base_address + static_cast<uint32_t>(offset), chunk_span);
    if (!res.has_value()) {
      std::cerr << "Write failed at offset " << offset << ": "
                << res.error().message() << "\n";
      client.shutdown();
      joinLoop();
      return 1;
    }
  }

  std::vector<double> latencies_us;
  latencies_us.reserve(ntimes);
  std::vector<uint8_t> read_buffer(total_bytes);

  updateProgress(0, ntimes);
  for (std::size_t iter = 0; iter < ntimes; ++iter) {
    const auto start_time = Clock::now();
    auto future = client.readAsync(
        target, base_address, static_cast<uint32_t>(total_bytes),
        [&read_buffer](spw_rmap::Packet packet) -> void {
          if (packet.data.size() != read_buffer.size()) {
            throw std::runtime_error("Unexpected data size in callback");
          }
          std::ranges::copy(packet.data, read_buffer.begin());
        });
    future.wait();
    auto res = future.get();
    const auto end_time = Clock::now();
    if (!res.has_value()) {
      std::cerr << "Read failed during iteration " << (iter + 1) << ": "
                << res.error().message() << "\n";
      client.shutdown();
      joinLoop();
      return 1;
    }

    if (!std::equal(read_buffer.begin(), read_buffer.end(), pattern.begin(),
                    pattern.end())) {
      std::cerr << "Data mismatch detected during iteration " << (iter + 1)
                << "\n";
      client.shutdown();
      joinLoop();
      return 1;
    }

    const auto elapsed =
        std::chrono::duration<double, std::nano>(end_time - start_time)
            .count();
    latencies_us.push_back(elapsed);
    updateProgress(iter + 1, ntimes);
  }
  std::cerr << '\n';

  auto mean = computeMean(latencies_us);
  auto stddev = computeStd(latencies_us, mean);
  auto [min_v, q1, median, q3, max_v] = computeQuartiles(latencies_us);

  std::cout << "mean=" << toMicroseconds(mean)
            << " std=" << toMicroseconds(stddev)
            << " min=" << toMicroseconds(min_v) << " q1=" << toMicroseconds(q1)
            << " median=" << toMicroseconds(median)
            << " q3=" << toMicroseconds(q3) << " max=" << toMicroseconds(max_v)
            << '\n';

  auto shutdown_res = client.shutdown();
  if (!shutdown_res.has_value()) {
    std::cerr << "Shutdown error: " << shutdown_res.error().message() << "\n";
    joinLoop();
    return 1;
  }

  joinLoop();
  return 0;
}
