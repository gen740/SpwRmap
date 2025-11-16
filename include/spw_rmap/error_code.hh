// Copyright (c) 2025 Gen
// Licensed under the MIT License. See LICENSE file for details.
#pragma once

#include <spw_rmap/packet_parser.hh>
#include <system_error>

namespace spw_rmap {

class StatusCodeCategory final : public std::error_category {
 public:
  [[nodiscard]] auto name() const noexcept -> const char* override {
    return "StatusCode";
  }

  [[nodiscard]] auto message(int ev) const -> std::string override {
    switch (static_cast<PacketParser::Status>(ev)) {
      case PacketParser::Status::Success:
        return "Success";
      case PacketParser::Status::InvalidPacket:
        return "Invalid packet";
      case PacketParser::Status::HeaderCRCError:
        return "Header CRC error";
      case PacketParser::Status::DataCRCError:
        return "Data CRC error";
      case PacketParser::Status::IncompletePacket:
        return "Incomplete packet";
      case PacketParser::Status::NotReplyPacket:
        return "Not a reply packet";
      case PacketParser::Status::PacketStatusError:
        return "Packet status error";
      case PacketParser::Status::UnknownProtocolIdentifier:
        return "Unknown protocol identifier";
      default:
        return "Unknown status code";
    }
  }
};

inline auto status_code_category() noexcept -> const std::error_category& {
  static StatusCodeCategory instance;
  return instance;
}

inline auto make_error_code(PacketParser::Status e) noexcept
    -> std::error_code {
  return {static_cast<int>(e), status_code_category()};
}

}  // namespace spw_rmap

namespace std {

template <>
struct is_error_code_enum<spw_rmap::PacketParser::Status> : true_type {};

}  // namespace std
