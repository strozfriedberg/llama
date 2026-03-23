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

bool operator==(const Dirent& l, const Dirent& r) {
  return l.Id == r.Id &&
         l.Path == r.Path &&
         l.Name == r.Name &&
         l.ShortName == r.ShortName &&
         l.Type == r.Type &&
         l.Flags == r.Flags &&
         l.MetaAddr == r.MetaAddr &&
         l.ParentAddr == r.ParentAddr &&
         l.MetaSeq == r.MetaSeq &&
         l.ParentSeq == r.ParentSeq;
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
  out.Id = "7819afd1142de937ea94fa2b34f53860a823c31447155a748b85f36a53191dfd";

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
  outB.Id = "4e4a548f1801a79393f9bc25baee4c2209cfee3f558c3ca254688f147b2f6bb5";

  REQUIRE(outB == dirents.pop());

  REQUIRE(!dirents.empty());
  REQUIRE("a" == dirents.top().Path);

  Dirent outA(makeDirent("a", "a"));
  outA.Id = "24e3bc15a787cbd19448a7e3ea0ba762bc50115f121c8e3261dbb023c9245eea";

  REQUIRE(outA == dirents.pop());
  REQUIRE(dirents.empty());
}

TEST_CASE("DirentStack dot and dotdot do not pollute paths") {
  RecordHasher rh;
  DirentStack dirents(rh);

  // Push a normal directory
  dirents.push(makeDirent("", "Users"));
  REQUIRE("Users" == dirents.top().Path);

  // Push "." — should not change path or stack depth
  Dirent dot(makeDirent("", "."));
  auto dotResult = dirents.push(std::move(dot));
  REQUIRE(dotResult.has_value());
  REQUIRE("Users" == dotResult->Path);
  REQUIRE("." == dotResult->Name);
  // Stack should still have just "Users"
  REQUIRE("Users" == dirents.top().Path);

  // Push ".." — should not change path or stack depth
  Dirent dotdot(makeDirent("", ".."));
  auto dotdotResult = dirents.push(std::move(dotdot));
  REQUIRE(dotdotResult.has_value());
  REQUIRE("Users" == dotdotResult->Path);
  REQUIRE(".." == dotdotResult->Name);
  // Stack should still have just "Users"
  REQUIRE("Users" == dirents.top().Path);

  // Push a normal child — path should be clean
  dirents.push(makeDirent("", "Default"));
  REQUIRE("Users/Default" == dirents.top().Path);

  // Pop back and verify paths are not polluted
  dirents.pop();
  REQUIRE("Users" == dirents.top().Path);
  dirents.pop();
  REQUIRE(dirents.empty());
}

