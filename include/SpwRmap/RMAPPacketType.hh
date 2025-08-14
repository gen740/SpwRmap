#pragma once

#include <cstdint>

namespace SpwRmap {

static constexpr uint8_t RMAPProtocolIdentifier = 0x01;

enum class RMAPPacketType : uint8_t {
  Command = 0b01000000,
  Reply = 0b00000000,
};

enum class RMAPCommandCode : uint8_t {
  Write = 0b00100000,                  // Read operation
  VerifyDataBeforeWrite = 0b00010000,  // Write operation
  Reply = 0b00001000,                  // Read-Modify-Write operation
  IncrementAddress = 0b00000100,       // Incremental address operation
};

// enum class RMAPCommandCode : uint8_t {
//   ReadSingleAddress = 0,
//   ReadIncrementingAddress = 0,
//   // ReadModifyWriteIncrementingAddress = 0,
//   WriteSingleAddressNoVerifyNoReply = 0,
//   WriteIncrementingAddressNoVerifyNoReply = 0,
//   WriteSingleAddressNoVerify = 0,
//   WriteIncrementingAddressNoVerify = 0,
//   WriteSingleAddressVerifyNoReply = 0,
//   WriteIncrementingAddressVerifyNoReply = 0,
//   WriteSingleAddressVerify = 0,
//   WriteIncrementingAddressVerify = 0,
// };

}  // namespace SpwRmap
