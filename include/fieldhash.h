#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

#include "hex.h"

struct FieldHash {
  std::array<uint8_t, 32> hash;

  bool operator==(const FieldHash& other) const {
    return hash == other.hash;
  }

  bool operator!=(const FieldHash& other) const {
    return !(*this == other);
  }

  std::string to_string() const {
    return hexEncode(hash.data(), hash.size());
  }
};
