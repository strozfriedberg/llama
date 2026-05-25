#include <catch2/catch_test_macros.hpp>

#include "hex.h"

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
#include <utility>

TEST_CASE("testHexEncode") {
  const std::vector<std::pair<std::vector<unsigned char>, std::string>> tests{
    {{0xFF}, "ff"},
    {{0x1c, 0x2d}, "1c2d"},
    {{0x0f, 0xf0, 0x02}, "0ff002"},
    {{0x00, 0x00, 0x00, 0x00}, "00000000"},
    {{0xE2, 0x49, 0x49, 0x32, 0xCF, 0x01, 0x9D, 0xC8, 0x40, 0x57, 0xF6, 0x48, 0x78, 0x92, 0x6D}, "e2494932cf019dc84057f64878926d"},
    {{0xAE, 0xED, 0x3A, 0xC7, 0x39, 0xD8, 0xFD, 0xDF, 0xCB, 0xD1, 0x91, 0x3B, 0x9E, 0x91, 0xE4}, "aeed3ac739d8fddfcbd1913b9e91e4"}
  };

  for (const auto& t : tests) {
    REQUIRE(t.second == hexEncode(&t.first[0], t.first.size()));
    REQUIRE(t.second == hexEncode(&t.first[0], &t.first[0] + t.first.size()));
  }
}

TEST_CASE("testHexDecodeRoundTrip") {
  // Known 32-byte value: encode to hex then decode back, must match original.
  const std::array<uint8_t, 32> original = {
    0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef,
    0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef,
    0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef,
    0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef
  };
  const std::string encoded = hexEncode(original.data(), original.size());
  REQUIRE(encoded == "1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef");

  const auto decoded = hexDecode<32>(encoded);
  REQUIRE(decoded == original);

  // Uppercase hex must also decode correctly.
  const auto decodedUpper = hexDecode<32>("1234567890ABCDEF1234567890ABCDEF1234567890ABCDEF1234567890ABCDEF");
  REQUIRE(decodedUpper == original);

  // Wrong length must throw.
  REQUIRE_THROWS_AS(hexDecode<32>("deadbeef"), std::invalid_argument);

  // Invalid character must throw.
  REQUIRE_THROWS_AS(
    hexDecode<4>("deadXX00"),
    std::invalid_argument);
}
