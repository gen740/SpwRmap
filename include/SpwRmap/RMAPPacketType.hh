#pragma once

#include <cstdint>

namespace SpwRmap {

enum class RMAPPacketType : uint8_t {
  Read = 0b00000000,                   // Read operation
  Write = 0b00100000,                  // Read operation
  VerifyDataBeforeWrite = 0b00010000,  // Write operation
  Reply = 0b00001000,                  // Read-Modify-Write operation
  IncrementAddress = 0b00000100,       // Incremental address operation
};

}
