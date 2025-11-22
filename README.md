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
