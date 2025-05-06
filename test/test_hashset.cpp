#include <catch2/catch_test_macros.hpp>

#include <hashset.h>

TEST_CASE("testHashsetBundleLookup") {
  SFHASH_HashValues Hashes;
  std::string blake3 = "4878ca0425c739fa427f7eda20fe845f6b2e46ba5fe2a14df5b1e32f50603215";
  sfhash_unhex(Hashes.Blake3, blake3.c_str(), blake3.size());

  LlamaHashset hsetBundle("test/hsets/blake3.hset");
  REQUIRE(hsetBundle.lookup(Hashes));
}