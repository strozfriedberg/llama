#include <catch2/catch_test_macros.hpp>

#include "direntstack.h"
#include "recordhasher.h"

std::ostream& operator<<(std::ostream& os, const Dirent& dirent) {
  os << "{\"Id\":\"" << dirent.Id << "\", \"Path\": \"" << dirent.Path << "\", \"Name\": \"" << dirent.Name << 
    "\", \"ShortName\": \"" << dirent.ShortName << "\", \"Type\": \"" << dirent.Type << "\", \"Flags\": \"" << dirent.Flags << 
    "\", \"MetaAddr\": " << dirent.MetaAddr << ", \"ParentAddr\": " << dirent.ParentAddr << ", \"MetaSeq\": " << dirent.MetaSeq << 
    ", \"ParentSeq\": " << dirent.ParentSeq << "}";
  return os;
}

TEST_CASE("testDirentStackStartsEmpty") {
  RecordHasher rh;
  DirentStack dirents(rh);
  REQUIRE(dirents.empty());
}

Dirent makeDirent(const std::string& path, const std::string& name) {
  return Dirent{
    "",
    path,
    name,
    "",
    "",
    "",
    0,
    0,
    0,
    0
  };
}

TEST_CASE("testDirentStackPushPop") {
  RecordHasher rh;
  DirentStack dirents(rh);

  Dirent in(makeDirent("", "the name"));

  dirents.push(std::move(in));
  
  REQUIRE(!dirents.empty());
  REQUIRE("the name" == dirents.top().Path);

  Dirent out(makeDirent("the name", "the name"));
  out.Id = "9725ac83ec80648192377bba20be829f7532953f50bfd735808ecc92a63ad011";

  REQUIRE(out == dirents.pop());
}

TEST_CASE("testDirentStackPushPushPopPop") {
  RecordHasher rh;
  DirentStack dirents(rh);

  REQUIRE(dirents.empty());

  Dirent a(makeDirent("", "a"));

  dirents.push(std::move(a));
  
  REQUIRE(!dirents.empty());
  REQUIRE("a" == dirents.top().Path);

  Dirent b(makeDirent("", "b"));

  dirents.push(std::move(b));

  REQUIRE(!dirents.empty());
  REQUIRE("a/b" == dirents.top().Path);

  Dirent outB(makeDirent("a/b", "b"));
  outB.Id = "43255526405934bbcf8c6b90f4f02d82e3e74e028b02942701be6354bf27677d";

  REQUIRE(outB == dirents.pop());

  REQUIRE(!dirents.empty());
  REQUIRE("a" == dirents.top().Path);

  Dirent outA(makeDirent("a", "a"));
  outA.Id = "fd88d31d3ec3b285f33ba011fe96290bd05d5272a9ef50ddae020c13ebe5319a";

  REQUIRE(outA == dirents.pop());
  REQUIRE(dirents.empty());
}

