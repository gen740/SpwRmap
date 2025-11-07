#pragma once

#include <cstdint>
#include <span>

namespace SpwRmap::CRC {

auto calcCRC(std::span<const uint8_t> data, uint8_t crc = 0x00) noexcept
    -> uint8_t;

};  // namespace SpwRmap::CRC
