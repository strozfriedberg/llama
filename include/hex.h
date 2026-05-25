#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

std::string hexEncode(const void* buf, size_t size);

std::string hexEncode(const void* beg, const void* end);

namespace detail {
  inline uint8_t hexNibble(char c) {
    if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
    if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(10 + (c - 'a'));
    if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(10 + (c - 'A'));
    throw std::invalid_argument(std::string("hexDecode: invalid hex character '") + c + "'");
  }
}

// Decode a hex string of exactly 2*N characters into a fixed-size byte array.
// Throws std::invalid_argument on wrong length or non-hex characters.
template<size_t N>
std::array<uint8_t, N> hexDecode(std::string_view hex) {
  if (hex.size() != 2 * N) {
    throw std::invalid_argument(
      "hexDecode: expected " + std::to_string(2 * N) +
      " hex chars, got " + std::to_string(hex.size()));
  }
  std::array<uint8_t, N> out{};
  for (size_t i = 0; i < N; ++i) {
    out[i] = static_cast<uint8_t>(
      (detail::hexNibble(hex[2 * i]) << 4) | detail::hexNibble(hex[2 * i + 1]));
  }
  return out;
}
