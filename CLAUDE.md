# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

- **Build**: `nix run .#build`
- **Lint and format**: `nix run .#check`
- **Development shell**: `nix develop`

Manual build (inside nix develop):
```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DSPWRMAP_BUILD_EXAMPLES=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
```

## Architecture

SpwRmap is a C++23 library providing a modern interface for SpaceWire RMAP (Remote Memory Access Protocol) communication. The library wraps the legacy SpaceWireRMAPLibrary using the PIMPL pattern to hide dependencies.

### Core Design

- **`SpwRmapBase`**: Abstract interface defining read/write operations, target node management, and timecode emission
- **`LegacySpwRmap`**: Production implementation using PIMPL to wrap legacy SpaceWireRMAPLibrary  
- **`DummySpwRmap`**: No-op implementation for testing
- **`TargetNode`**: Contains logical address, target SpaceWire address, and reply address

The library uses C++23 features including `std::span` for memory-safe buffer handling. Legacy dependencies are patched to remove `throw()` specifications for C++17 compatibility.

## Key Files

- `include/SpwRmap/SpwRmapBase.hh`: Core interface
- `src/LegacySpwRmap.cc`: Main implementation with PIMPL class
- `flake.nix`: Nix build configuration and dependency management