# spw_rmap

`spw_rmap` is a SpaceWire/RMAP helper library that provides packet builders/parsers, a TCP transport, CLI utilities, and Python bindings.

## Building

```bash
cmake -S . -B build
cmake --build build
```

Key CMake options:

- `SPWRMAP_BUILD_APPS` (default `ON`): build the `spwrmap` and `spwrmap_speedtest` CLI tools.
- `SPWRMAP_BUILD_EXAMPLES` (default `OFF`): enable examples under `examples/`.
- `SPWRMAP_BUILD_TESTS` (default `ON`): add the `tests` subdirectory and register the GTest suite.
- `SPWRMAP_BUILD_PYTHON_BINDINGS` (default `OFF`): build the pybind11 module (also enabled when using `pyproject.toml` / `scikit-build-core`).

## Testing

```bash
cmake --build build --target spwrmap_tests
cd build
ctest --output-on-failure
```

Some TCP tests require the ability to bind a local port; they will be skipped automatically when the environment forbids that operation (e.g., in sandboxed CI).

## Python bindings

To build the wheel:

```bash
python -m pip install .  # uses pyproject + scikit-build-core
```

The resulting package exposes `_core.SpwRmapTCPNode` mirroring the C++ API.

## Key Concepts

- `target_node`: abstraction describing a SpaceWire node address (logical address, SpaceWire hop list, reply path). Implemented by `TargetNodeBase` with fixed/dynamic variants so the same transport can talk to different hardware endpoints.
- `tcp_node`: the SpaceWire-over-TCP bridge (`SpwRmapTCPClient`/`SpwRmapTCPServer`) that owns the sockets, buffers, and RMAP transaction management.
- `write` / `read`: synchronous helpers that perform the transaction, block until a reply arrives (or timeout/retry), and return `std::expected` success/error codes.
- `writeAsync` / `readAsync`: asynchronous variants returning `std::future` that resolve when the reply is received; they invoke user-supplied callbacks before fulfilling the future so event-driven integrations can react immediately.

See `examples/spwrmap_example.cc` (C++) and `examples/spwrmap_example.py` (Python) for minimal workflows demonstrating how to connect, construct a target node, and issue read/write RMAP commands.

# Quick Start Guide

## C++

### Initialize spw

```cpp
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include <spw_rmap/spw_rmap_tcp_node.hh>
#include <spw_rmap/target_node.hh>

int main() {
  using namespace std::chrono_literals;

  spw_rmap::SpwRmapTCPClient client(
      {.ip_address = "127.0.0.1", .port = "10030"});

  client.setInitiatorLogicalAddress(0xFE);
  client.connect(500ms).value();  // abort on failure

  std::thread loop([&client] {
    auto res = client.runLoop();
    if (!res) {
      throw std::system_error(res.error());
    }
  });

  // ...

  client.shutdown();
  if (loop.joinable()) {
    loop.join();
  }
}
```

You can also call `poll()` manually from your own loop instead of spawning a thread.

### Creating target node

```cpp
auto target = std::make_shared<spw_rmap::TargetNodeDynamic>(
    /*logical_address=*/0x34,
    std::vector<uint8_t>{3, 5, 7},        // SpaceWire hops
    std::vector<uint8_t>{9, 11, 13, 0x0}  // Reply path
);
```

`TargetNodeFixed<N,M>` is available when the address sizes are known at compile time.

### Read and write

```cpp
std::array<uint8_t, 4> write_payload{0x12, 0x34, 0x56, 0x78};
client.write(target, /*address=*/0x20000000, write_payload).value();

std::array<uint8_t, 4> read_buffer{};
client.read(target, 0x20000000, std::span(read_buffer)).value();

auto read_future =
    client.readAsync(target, 0x20000000, /*length=*/4,
                     [](spw_rmap::Packet packet) {
                       std::cout << "Async read returned "
                                 << packet.data.size() << " bytes\n";
                     });
read_future.get().value();

auto write_future =
    client.writeAsync(target, 0x20000000, std::span(write_payload),
                      [](const spw_rmap::Packet&) {
                        std::cout << "Async write acknowledged\n";
                      });
write_future.get().value();
```

`write`/`read` are *synchronous*: they transmit the command, block until a reply is parsed (with retries/timeouts handled internally), and return `std::expected`.  
`writeAsync`/`readAsync` are *asynchronous*: they enqueue the transaction, immediately return a `std::future`, and invoke the supplied callback as soon as the reply arrives—before the future resolves—allowing low-latency event handling.

## Python

### Initialize spw

```python
from pyspw_rmap import _core as spw

node = spw.SpwRmapTCPNode("127.0.0.1", "10030")
node.start()  # connects and launches runLoop internally
```

Call `node.stop()` to shut down and join the worker thread. Manual polling isn’t exposed in Python; launch `start()` once and let the worker handle replies.

### Creating target node

```python
target = spw.TargetNode()
target.logical_address = 0x34
target.target_spacewire_address = [3, 5, 7]
target.reply_address = [9, 11, 13, 0]
```

### Read and write

```python
# blocking write/read; no async API is exposed in Python
node.write(target, 0x20000000, [0x12, 0x34, 0x56, 0x78])
data = node.read(target, 0x20000000, 4)
print("sync read:", list(data))

node.stop()
```

Python bindings currently offer only synchronous `read`/`write` methods. To parallelize operations you must call them from your own threads or processes; there is no built-in async wrapper.

The [examples](examples) directory contains CLI programs that parse command-line arguments, manage the lifecycle for you, and show additional patterns (speed tests, multi-target setups, etc.).
