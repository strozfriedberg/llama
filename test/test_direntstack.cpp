#include <catch2/catch_test_macros.hpp>

#include "direntstack.h"
#include "recordhasher.h"

std::ostream& operator<<(std::ostream& os, const Dirent& dirent) {
  os << "{\"Id\":\"" << dirent.Id << "\", \"Path\": \"" << dirent.Path << "\", \"Name\": \"" << dirent.Name << 
    "\", \"Shrt_name\": \"" << dirent.Shrt_name << "\", \"Type\": \"" << dirent.Type << "\", \"Flags\": \"" << dirent.Flags << 
    "\", \"Meta_addr\": " << dirent.Meta_addr << ", \"Par_addr\": " << dirent.Par_addr << ", \"Meta_seq\": " << dirent.Meta_seq << 
    ", \"Par_seq\": " << dirent.Par_seq << "}";
  return os;
}

TEST_CASE("testDirentStackStartsEmpty") {
  RecordHasher rh;
  DirentStack dirents(rh);
  REQUIRE(dirents.empty());
}

TEST_CASE("testDirentStackPushPop") {
  RecordHasher rh;
  DirentStack dirents(rh);

  Dirent in("", "the name");

  dirents.push(std::move(in));
  
  REQUIRE(!dirents.empty());
  REQUIRE("the name" == dirents.top().Path);

  Dirent out("the name", "the name");
  out.Id = "9725ac83ec80648192377bba20be829f7532953f50bfd735808ecc92a63ad011";

  REQUIRE(out == dirents.pop());
}

TEST_CASE("testDirentStackPushPushPopPop") {
  RecordHasher rh;
  DirentStack dirents(rh);

  REQUIRE(dirents.empty());

  Dirent a("", "a");

  dirents.push(std::move(a));
  
  REQUIRE(!dirents.empty());
  REQUIRE("a" == dirents.top().Path);

  Dirent b("", "b");

  dirents.push(std::move(b));

  REQUIRE(!dirents.empty());
  REQUIRE("a/b" == dirents.top().Path);

  Dirent outB("a/b", "b");
  outB.Id = "43255526405934bbcf8c6b90f4f02d82e3e74e028b02942701be6354bf27677d";

  REQUIRE(outB == dirents.pop());

  REQUIRE(!dirents.empty());
  REQUIRE("a" == dirents.top().Path);

  Dirent outA("a", "a");
  outA.Id = "fd88d31d3ec3b285f33ba011fe96290bd05d5272a9ef50ddae020c13ebe5319a";

  REQUIRE(outA == dirents.pop());
  REQUIRE(dirents.empty());
}

