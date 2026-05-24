#include <catch2/catch_test_macros.hpp>

#include "direntstack.h"
#include "fieldhash.h"
#include "recordhasher.h"

std::ostream& operator<<(std::ostream& os, const Dirent& dirent) {
  os << "{\"Id\":\"" << dirent.Id << "\", \"Path\": \"" << dirent.Path << "\", \"Name\": \"" << dirent.Name <<
    "\", \"ShortName\": \"" << dirent.ShortName << "\", \"Type\": \"" << dirent.Type << "\", \"Flags\": \"" << dirent.Flags <<
    "\", \"MetaAddr\": " << dirent.MetaAddr << ", \"ParentAddr\": " << dirent.ParentAddr << ", \"MetaSeq\": " << dirent.MetaSeq <<
    ", \"ParentSeq\": " << dirent.ParentSeq <<
    ", \"MetaId\": \"" << dirent.MetaId << "\", \"ParentId\": \"" << dirent.ParentId << "\"}";
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
         l.ParentSeq == r.ParentSeq &&
         l.MetaId == r.MetaId &&
         l.ParentId == r.ParentId;
}

TEST_CASE("testDirentStackStartsEmpty") {
  RecordHasher rh;
  DirentStack dirents(rh);
  REQUIRE(dirents.empty());
}

Dirent makeDirent(const std::string& path, const std::string& name) {
  Dirent d{};
  d.Path = path;
  d.Name = name;
  return d;
}

TEST_CASE("testDirentStackPushPop") {
  RecordHasher rh;
  DirentStack dirents(rh);
  dirents.setFsContext("disk.E01", 1048576);

  Dirent in(makeDirent("", "the name"));
  in.MetaAddr = 100;
  in.MetaSeq = 1;
  in.ParentAddr = 5;
  in.ParentSeq = 5;

  dirents.push(std::move(in));

  REQUIRE(!dirents.empty());
  REQUIRE("the name" == dirents.top().Path);

  Dirent out = dirents.pop();
  REQUIRE(out.Path == "the name");
  REQUIRE(out.Name == "the name");
  REQUIRE_FALSE(out.Id.empty());
  REQUIRE(out.Id == "97ece597bbca827515097c1f8ff18ca2e4170fca28577b152ca0ae9a518b9672");
  REQUIRE_FALSE(out.MetaId.empty());
  REQUIRE_FALSE(out.ParentId.empty());

  // MetaId must equal hashInodeIdentity over the same inputs.
  REQUIRE(out.MetaId == rh.hashInodeIdentity("disk.E01", 1048576, 100, 1).to_string());

  // ParentId likewise.
  REQUIRE(out.ParentId == rh.hashInodeIdentity("disk.E01", 1048576, 5, 5).to_string());
}

TEST_CASE("testDirentStackPushPushPopPop") {
  RecordHasher rh;
  DirentStack dirents(rh);
  dirents.setFsContext("", 0);

  REQUIRE(dirents.empty());

  Dirent a(makeDirent("", "a"));

  dirents.push(std::move(a));

  REQUIRE(!dirents.empty());
  REQUIRE("a" == dirents.top().Path);

  Dirent b(makeDirent("", "b"));

  dirents.push(std::move(b));

  REQUIRE(!dirents.empty());
  REQUIRE("a/b" == dirents.top().Path);

  const std::string zeroId = rh.hashInodeIdentity("", 0, 0, 0).to_string();

  Dirent outB(makeDirent("a/b", "b"));
  outB.Id = "4e4a548f1801a79393f9bc25baee4c2209cfee3f558c3ca254688f147b2f6bb5";
  outB.MetaId = zeroId;
  outB.ParentId = zeroId;

  REQUIRE(outB == dirents.pop());

  REQUIRE(!dirents.empty());
  REQUIRE("a" == dirents.top().Path);

  Dirent outA(makeDirent("a", "a"));
  outA.Id = "24e3bc15a787cbd19448a7e3ea0ba762bc50115f121c8e3261dbb023c9245eea";
  outA.MetaId = zeroId;
  outA.ParentId = zeroId;

  REQUIRE(outA == dirents.pop());
  REQUIRE(dirents.empty());
}

TEST_CASE("DirentStack dot and dotdot do not pollute paths") {
  RecordHasher rh;
  DirentStack dirents(rh);
  dirents.setFsContext("", 0);

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

