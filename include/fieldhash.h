#pragma once

#include <algorithm>
#include <cstdint>

struct FieldHash {
  uint8_t hash[32];

  bool operator==(const FieldHash& other) const {
    return std::equal(hash, hash + sizeof(hash), other.hash);
  }

  bool operator!=(const FieldHash& other) const {
    return !(*this == other);
  }
};

