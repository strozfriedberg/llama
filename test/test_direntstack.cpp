#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <string>

#include "direntstack.h"
#include "fieldhash.h"
#include "hex.h"
#include "recordhasher.h"


std::ostream& operator<<(std::ostream& os, const Dirent& dirent) {
  os << "{\"Id\":\"" << hexEncode(dirent.Id.data(), dirent.Id.size())
     << "\", \"Path\": \"" << dirent.Path << "\", \"Name\": \"" << dirent.Name <<
    "\", \"ShortName\": \"" << dirent.ShortName << "\", \"Type\": \"" << dirent.Type << "\", \"Flags\": \"" << dirent.Flags <<
    "\", \"MetaAddr\": " << dirent.MetaAddr << ", \"ParentAddr\": " << dirent.ParentAddr << ", \"MetaSeq\": " << dirent.MetaSeq <<
    ", \"ParentSeq\": " << dirent.ParentSeq <<
    ", \"MetaId\": \"" << hexEncode(dirent.MetaId.data(), dirent.MetaId.size())
    << "\", \"ParentId\": \"" << hexEncode(dirent.ParentId.data(), dirent.ParentId.size()) << "\"}";
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
  REQUIRE(out.Id != std::array<uint8_t, 32>{});
  REQUIRE(out.Id == hexDecode<32>("97ece597bbca827515097c1f8ff18ca2e4170fca28577b152ca0ae9a518b9672"));
  REQUIRE(out.MetaId != std::array<uint8_t, 32>{});
  REQUIRE(out.ParentId != std::array<uint8_t, 32>{});

  // MetaId must equal hashInodeIdentity over the same inputs.
  REQUIRE(out.MetaId == rh.hashInodeIdentity("disk.E01", 1048576, 100, 1).hash);

  // ParentId likewise.
  REQUIRE(out.ParentId == rh.hashInodeIdentity("disk.E01", 1048576, 5, 5).hash);
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

  const std::array<uint8_t, 32> zeroId = rh.hashInodeIdentity("", 0, 0, 0).hash;

  Dirent outB(makeDirent("a/b", "b"));
  outB.Id = hexDecode<32>("4e4a548f1801a79393f9bc25baee4c2209cfee3f558c3ca254688f147b2f6bb5");
  outB.MetaId = zeroId;
  outB.ParentId = zeroId;

  REQUIRE(outB == dirents.pop());

  REQUIRE(!dirents.empty());
  REQUIRE("a" == dirents.top().Path);

  Dirent outA(makeDirent("a", "a"));
  outA.Id = hexDecode<32>("24e3bc15a787cbd19448a7e3ea0ba762bc50115f121c8e3261dbb023c9245eea");
  outA.MetaId = zeroId;
  outA.ParentId = zeroId;

  REQUIRE(outA == dirents.pop());
  REQUIRE(dirents.empty());
}

TEST_CASE("DirentStack dot and dotdot do not pollute paths") {
  RecordHasher rh;
  DirentStack dirents(rh);
  dirents.setFsContext("disk.E01", 1048576);

  // Push a normal directory
  dirents.push(makeDirent("", "Users"));
  REQUIRE("Users" == dirents.top().Path);

  // Push "." — should not change path or stack depth
  Dirent dot(makeDirent("", "."));
  dot.MetaAddr = 42;
  dot.MetaSeq = 1;
  dot.ParentAddr = 42;
  dot.ParentSeq = 1;
  auto dotResult = dirents.push(std::move(dot));
  REQUIRE(dotResult.has_value());
  REQUIRE("Users" == dotResult->Path);
  REQUIRE("." == dotResult->Name);
  // Id/MetaId/ParentId are populated, matching the same hashing pipeline
  // that pop() uses for ordinary dirents.
  REQUIRE(dotResult->Id != std::array<uint8_t, 32>{});
  REQUIRE(dotResult->Id == rh.hashDirent(*dotResult).hash);
  REQUIRE(dotResult->MetaId == rh.hashInodeIdentity("disk.E01", 1048576, 42, 1).hash);
  REQUIRE(dotResult->ParentId == rh.hashInodeIdentity("disk.E01", 1048576, 42, 1).hash);
  // Stack should still have just "Users"
  REQUIRE("Users" == dirents.top().Path);

  // Push ".." — should not change path or stack depth
  Dirent dotdot(makeDirent("", ".."));
  dotdot.MetaAddr = 7;
  dotdot.MetaSeq = 1;
  dotdot.ParentAddr = 42;
  dotdot.ParentSeq = 1;
  auto dotdotResult = dirents.push(std::move(dotdot));
  REQUIRE(dotdotResult.has_value());
  REQUIRE("Users" == dotdotResult->Path);
  REQUIRE(".." == dotdotResult->Name);
  REQUIRE(dotdotResult->Id != std::array<uint8_t, 32>{});
  REQUIRE(dotdotResult->Id == rh.hashDirent(*dotdotResult).hash);
  REQUIRE(dotdotResult->MetaId == rh.hashInodeIdentity("disk.E01", 1048576, 7, 1).hash);
  REQUIRE(dotdotResult->ParentId == rh.hashInodeIdentity("disk.E01", 1048576, 42, 1).hash);
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

TEST_CASE("setFsContext throws if stack not drained") {
  RecordHasher rh;
  DirentStack dirents(rh);
  dirents.setFsContext("fs1.E01", 1048576);

  Dirent in(makeDirent("", "leftover"));
  in.MetaAddr = 5;
  dirents.push(std::move(in));
  REQUIRE_FALSE(dirents.empty());

  // Flipping context with a non-empty stack would silently corrupt the
  // FK hashes of the residual dirents in release builds; require that
  // callers drain first.
  REQUIRE_THROWS(dirents.setFsContext("fs2.E01", 2097152));
}

TEST_CASE("dirents popped after context flip hash under the new context") {
  RecordHasher rh;
  DirentStack dirents(rh);

  // First FS context: push a dirent and pop under fs1.
  dirents.setFsContext("fs1.E01", 1048576);
  Dirent a(makeDirent("", "alpha"));
  a.MetaAddr = 100;
  a.MetaSeq = 1;
  a.ParentAddr = 5;
  a.ParentSeq = 5;
  dirents.push(std::move(a));
  Dirent outA = dirents.pop();
  REQUIRE(outA.MetaId == rh.hashInodeIdentity("fs1.E01", 1048576, 100, 1).hash);

  // Flip context now that the stack is drained.
  dirents.setFsContext("fs2.E01", 2097152);

  // Second FS: same MetaAddr/MetaSeq must produce a DIFFERENT MetaId
  // because the context contributes to the hash.
  Dirent b(makeDirent("", "alpha"));
  b.MetaAddr = 100;
  b.MetaSeq = 1;
  b.ParentAddr = 5;
  b.ParentSeq = 5;
  dirents.push(std::move(b));
  Dirent outB = dirents.pop();
  REQUIRE(outB.MetaId == rh.hashInodeIdentity("fs2.E01", 2097152, 100, 1).hash);
  REQUIRE(outA.MetaId != outB.MetaId);
}
