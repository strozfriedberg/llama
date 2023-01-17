#include <catch2/catch_test_macros.hpp>

#include "direntstack.h"
#include "recordhasher.h"

TEST_CASE("testDirentStackStartsEmpty") {
  RecordHasher rh;
  DirentStack dirents(rh);
  REQUIRE(dirents.empty());
}

TEST_CASE("testDirentStackPushPop") {
  RecordHasher rh;
  DirentStack dirents(rh);

  jsoncons::json in(
    jsoncons::json_object_arg,
    {
      { "foo", "bar" },
      { "type", "whatever" }
    }
  );
  
  dirents.push("filename", std::move(in));
  
  REQUIRE(!dirents.empty());
  REQUIRE("filename" == dirents.top()["path"]);
  
  const jsoncons::json exp_out(
    jsoncons::json_object_arg,
    {
      { "foo", "bar" },
      { "children", jsoncons::json_array_arg },
      { "streams", jsoncons::json_array_arg },
      { "hash", "2cf245061b41742690f0ebe9eceba6d8763d24d948f8969cae2071412bbc610e" },
      { "path", "filename" },
      { "type", "whatever" },
    }
  );

  REQUIRE(exp_out == dirents.pop());
}

TEST_CASE("testDirentStackPushPushPopPop") {
  RecordHasher rh;
  DirentStack dirents(rh);

  REQUIRE(dirents.empty());

  jsoncons::json a(
    jsoncons::json_object_arg,
    {
      { "foo", "bar" },
      { "type", "whatever" }
    }
  );
  
  dirents.push("a", std::move(a));
  
  REQUIRE(!dirents.empty());
  REQUIRE("a" == dirents.top()["path"]);

  jsoncons::json b(
    jsoncons::json_object_arg,
    {
      { "foo", "baz" },
      { "type", "whatever" }
    }
  );
 
  dirents.push("b", std::move(b));

  REQUIRE(!dirents.empty());
  REQUIRE("a/b" == dirents.top()["path"]);

  const jsoncons::json exp_out_b(
    jsoncons::json_object_arg,
    {
      { "foo", "baz" },
      { "children", jsoncons::json_array_arg },
      { "streams", jsoncons::json_array_arg },
      { "hash", "d41f36222eabcc685f01bbd288b60b66ff41dc4b52c69ba4a5126a74b4489ac7" },
      { "path", "a/b" },
      { "type", "whatever" },
    }
  );

  REQUIRE(exp_out_b == dirents.pop());

  REQUIRE(!dirents.empty());
  REQUIRE("a" == dirents.top()["path"]);

  const jsoncons::json exp_out_a(
    jsoncons::json_object_arg,
    {
      { "foo", "bar" },
      {
        "children",
        jsoncons::json(
          jsoncons::json_array_arg,
          { "d41f36222eabcc685f01bbd288b60b66ff41dc4b52c69ba4a5126a74b4489ac7" }
        )
      },
      { "streams", jsoncons::json_array_arg },
      { "hash", "0907b14d225552a966673312558b413135edd21d0460164141a1f74274f3281a" },
      { "path", "a" },
      { "type", "whatever" },
    }
  );

  REQUIRE(exp_out_a == dirents.pop());
  REQUIRE(dirents.empty());
}
