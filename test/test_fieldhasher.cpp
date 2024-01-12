#include <catch2/catch_test_macros.hpp>

#include "fieldhasher.h"

TEST_CASE("testHash") {
  const FieldHash exp{{
    0xb0, 0xa3, 0xf8, 0xf8, 0x32, 0x2c, 0xd7, 0xe6,
    0x59, 0x02, 0x48, 0x01, 0xd0, 0xea, 0x72, 0x04,
    0xbe, 0x35, 0x74, 0x17, 0x1a, 0x2b, 0x50, 0x33,
    0x23, 0x74, 0xc1, 0x5b, 0xd4, 0xf1, 0xd1, 0x70
  }};

  FieldHasher hasher;

  // piecewise
  hasher.hash_it(1);
  hasher.hash_it(26);
  hasher.hash_it("foo");
  hasher.hash_it(std::string("bar"));
  hasher.hash_it(-0.35);
  REQUIRE(exp == hasher.get_hash());

  // all in one go
  REQUIRE(exp == hasher.hash(1, 26, "foo", std::string("bar"), -0.35));
}