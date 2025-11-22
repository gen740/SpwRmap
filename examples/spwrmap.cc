#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "spw_rmap/spw_rmap_tcp_node.hh"
#include "spw_rmap/target_node.hh"

using namespace std::chrono_literals;

namespace {

struct Options {
  std::string ip{"127.0.0.1"};
  std::string port{"10030"};
  std::string type;
  std::vector<uint8_t> target_address;
  std::vector<uint8_t> reply_address;
  std::optional<uint32_t> address;
  std::optional<std::size_t> length;
  std::vector<uint8_t> data;
};

constexpr uint8_t kInitiatorLogicalAddress = 0xFE;
constexpr uint8_t kTargetLogicalAddress = 0xFE;

void printUsage(const char* program) {
  std::cerr
      << "Usage: " << program
      << " --type <read|write> --ip <addr> --port <port>\n"
      << "           --target-address <byte...> --reply-address <byte...>\n"
      << "           --address <addr> [--length <bytes>] [--data <byte...>]\n\n"
      << "Examples:\n"
      << "  " << program
      << " --type read --target-address 3 6 --reply-address 2 6\n"
      << "           --address 0x44a2006C --length 4\n"
      << "  " << program
      << " --type write --target-address 3 6 2 --reply-address 2 6 4\n"
      << "           --address 0x44a2006C --data 0x32 0x42 0x18 0x34\n";
}

auto parseUnsigned(std::string_view token, unsigned long long max_value)
    -> std::optional<unsigned long long> {
  try {
    std::string value_str(token);
    size_t idx = 0;
    auto value = std::stoull(value_str, &idx, 0);
    if (idx != value_str.size() || value > max_value) {
      return std::nullopt;
    }
    return value;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

auto parseByteSequence(int argc, char** argv, int& index,
                       std::string_view option_name,
                       std::vector<uint8_t>& destination) -> bool {
  bool parsed = false;
  while (index + 1 < argc) {
    std::string_view next = argv[index + 1];
    if (next.starts_with("--")) {
      break;
    }
    ++index;
    auto value = parseUnsigned(next, 0xFF);
    if (!value.has_value()) {
      std::cerr << "Invalid value for --" << option_name << ": '" << next
                << "'\n";
      return false;
    }
    destination.push_back(static_cast<uint8_t>(*value));
    parsed = true;
  }
  if (!parsed) {
    std::cerr << "--" << option_name << " requires at least one byte value.\n";
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
    const auto name = arg.substr(2);

    auto takeValue =
        [&](std::string_view option) -> std::optional<std::string> {
      if (i + 1 >= argc) {
        std::cerr << "--" << option << " requires a value.\n";
        return std::nullopt;
      }
      return std::string(argv[++i]);
    };

    if (name == "ip") {
      auto value = takeValue(name);
      if (!value) {
        return std::nullopt;
      }
      opts.ip = std::move(*value);
    } else if (name == "port") {
      auto value = takeValue(name);
      if (!value) {
        return std::nullopt;
      }
      opts.port = std::move(*value);
    } else if (name == "type") {
      auto value = takeValue(name);
      if (!value) {
        return std::nullopt;
      }
      opts.type = std::move(*value);
      std::transform(
          opts.type.begin(), opts.type.end(), opts.type.begin(),
          [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    } else if (name == "target-address") {
      if (!parseByteSequence(argc, argv, i, name, opts.target_address)) {
        return std::nullopt;
      }
    } else if (name == "reply-address") {
      if (!parseByteSequence(argc, argv, i, name, opts.reply_address)) {
        return std::nullopt;
      }
    } else if (name == "address") {
      auto value = takeValue(name);
      if (!value) {
        return std::nullopt;
      }
      auto parsed = parseUnsigned(*value, std::numeric_limits<uint32_t>::max());
      if (!parsed.has_value()) {
        std::cerr << "Invalid --address: '" << *value << "'\n";
        return std::nullopt;
      }
      opts.address = static_cast<uint32_t>(*parsed);
    } else if (name == "length") {
      auto value = takeValue(name);
      if (!value) {
        return std::nullopt;
      }
      auto parsed =
          parseUnsigned(*value, std::numeric_limits<std::size_t>::max());
      if (!parsed.has_value() || *parsed == 0) {
        std::cerr << "Invalid --length: '" << *value << "'\n";
        return std::nullopt;
      }
      opts.length = static_cast<std::size_t>(*parsed);
    } else if (name == "data") {
      if (!parseByteSequence(argc, argv, i, name, opts.data)) {
        return std::nullopt;
      }
    } else {
      std::cerr << "Unknown option: --" << name << "\n";
      return std::nullopt;
    }
  }

  if (opts.type != "read" && opts.type != "write") {
    std::cerr << "--type must be 'read' or 'write'.\n";
    return std::nullopt;
  }
  if (opts.target_address.empty()) {
    std::cerr << "--target-address is required.\n";
    return std::nullopt;
  }
  if (opts.reply_address.empty()) {
    std::cerr << "--reply-address is required.\n";
    return std::nullopt;
  }
  if (!opts.address.has_value()) {
    std::cerr << "--address is required.\n";
    return std::nullopt;
  }
  if (opts.type == "read" && !opts.length.has_value()) {
    std::cerr << "--length is required for read operations.\n";
    return std::nullopt;
  }
  if (opts.type == "write" && opts.data.empty()) {
    std::cerr << "--data is required for write operations.\n";
    return std::nullopt;
  }

  return opts;
}

auto performRead(const Options& opts, spw_rmap::SpwRmapTCPClient& client,
                 const std::shared_ptr<spw_rmap::TargetNodeBase>& target)
    -> bool {
  std::vector<uint8_t> buffer(*opts.length);
  auto res = client.read(target, *opts.address, buffer);
  if (!res.has_value()) {
    std::cerr << "Read failed: " << res.error().message() << "\n";
    return false;
  }
  std::cout << "Read " << buffer.size() << " bytes from 0x" << std::hex
            << std::setw(8) << std::setfill('0') << *opts.address << ":";
  for (const auto byte : buffer) {
    std::cout << " 0x" << std::setw(2) << static_cast<int>(byte);
  }
  std::cout << std::dec << std::setfill(' ') << "\n";
  return true;
}

auto performWrite(const Options& opts, spw_rmap::SpwRmapTCPClient& client,
                  const std::shared_ptr<spw_rmap::TargetNodeBase>& target)
    -> bool {
  auto res = client.write(target, *opts.address, opts.data);
  if (!res.has_value()) {
    std::cerr << "Write failed: " << res.error().message() << "\n";
    return false;
  }
  std::cout << "Wrote " << opts.data.size() << " bytes to 0x" << std::hex
            << std::setw(8) << std::setfill('0') << *opts.address << std::dec
            << " successfully.\n";
  return true;
}

}  // namespace

auto main(int argc, char** argv) -> int {
  if (argc == 1) {
    printUsage(argv[0]);
    return 1;
  }
  for (int i = 1; i < argc; ++i) {
    if (std::string_view(argv[i]) == "--help") {
      printUsage(argv[0]);
      return 0;
    }
  }

  auto options = parseOptions(argc, argv);
  if (!options) {
    printUsage(argv[0]);
    return 1;
  }

  auto opts = std::move(*options);

  auto client =
      spw_rmap::SpwRmapTCPClient({.ip_address = opts.ip, .port = opts.port});
  client.setInitiatorLogicalAddress(kInitiatorLogicalAddress);

  auto connect_res = client.connect(1s);
  if (!connect_res.has_value()) {
    std::cerr << "Failed to connect to " << opts.ip << ":" << opts.port << " - "
              << connect_res.error().message() << "\n";
    return 1;
  }

  std::thread loop_thread([&client]() -> void {
    auto res = client.runLoop();
    if (!res.has_value()) {
      std::cerr << "runLoop error: " << res.error().message() << "\n";
    }
  });

  auto target_node = std::make_shared<spw_rmap::TargetNodeDynamic>(
      kTargetLogicalAddress, std::move(opts.target_address),
      std::move(opts.reply_address));

  bool success = opts.type == "read" ? performRead(opts, client, target_node)
                                     : performWrite(opts, client, target_node);

  auto shutdown_res = client.shutdown();
  if (!shutdown_res.has_value()) {
    std::cerr << "Shutdown error: " << shutdown_res.error().message() << "\n";
    success = false;
  }
  loop_thread.join();
  return success ? 0 : 1;
}
