#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

class BitsetVar {
public:
  void push_back(bool value) {
    if (Size % 8 == 0) Bits.push_back(0);
    if (value) Bits.back() |= uint8_t(1u << (Size % 8));
    ++Size;
  }

  bool get(size_t index) const {
    return Bits[index / 8] & uint8_t(1u << (index % 8));
  }

  void set(size_t index, bool value) {
    const auto mask = uint8_t(1u << (index % 8));
    if (value) Bits[index / 8] |= mask;
    else       Bits[index / 8] &= uint8_t(~mask);
  }

  size_t size() const { return Size; }
  void clear() { Bits.clear(); Size = 0; }
  void reserve(size_t n) { Bits.reserve((n + 7) / 8); }

private:
  std::vector<uint8_t> Bits;
  size_t Size = 0;
};
