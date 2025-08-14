#pragma once

#include <cstdint>
#include <span>

namespace SpwRmap::CRC {

auto calcCRC(std::span<const uint8_t> data) noexcept -> uint8_t;

};  // namespace SpwRmap::CRC
