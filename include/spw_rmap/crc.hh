// Copyright (c) 2025 Gen
// Licensed under the MIT License. See LICENSE file for details.
#pragma once

#include <cstdint>
#include <span>

namespace spw_rmap::crc {

/**
 * @brief Calculate the CRC for the given data.
 *
 * @param data The input data for which the CRC is to be calculated.
 * @param crc The initial CRC value (default is 0x00).
 *
 * @return uint8_t The calculated CRC value.
 */
auto calcCRC(std::span<const uint8_t> data, uint8_t crc = 0x00) noexcept
    -> uint8_t;

};  // namespace spw_rmap::crc
