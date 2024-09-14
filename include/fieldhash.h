#pragma once

#include <algorithm>
#include <cstdint>

#include "hex.h"

struct FieldHash {
  uint8_t hash[32];

  bool operator==(const FieldHash& other) const {
    return std::equal(hash, hash + sizeof(hash), other.hash);
  }

  bool operator!=(const FieldHash& other) const {
    return !(*this == other);
  }

  std::string to_string() const {
    return hexEncode(hash, 32);
  }
};

