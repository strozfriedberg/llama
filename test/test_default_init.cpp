#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstring>
#include <new>

#include "direntbatch.h"
#include "ducksig.h"
#include "duckhash.h"
#include "entry.h"
#include "evidencerec.h"
#include "extent.h"
#include "inode.h"
#include "llamabatch.h"

// These tests guard the in-class initializers on every std::array<uint8_t, N>
// field across the structs the binary-hash migration flipped. Without an
// in-class `{}` initializer, a struct constructed via default-initialization
// (e.g. `Dirent d;`) leaves trivial members holding indeterminate bytes — see
// szechuan QC, where `.` and `..` dirents leaked uninitialized stack memory
// into the dirent table. Each test pre-fills a buffer with a non-zero pattern,
// placement-news the struct over it with default-init, and asserts the
// byte-array fields read back as all-zero.

namespace {

template<typename T, typename F>
void withDefaultInitOverDirtyMemory(F&& fn) {
  alignas(T) std::byte buf[sizeof(T)];
  std::memset(buf, 0xCC, sizeof(buf));
  T* p = ::new (static_cast<void*>(buf)) T;
  fn(*p);
  p->~T();
}

constexpr std::array<uint8_t, 32> Zero32{};

} // namespace

TEST_CASE("Dirent default-init zeros Id, MetaId, ParentId", "[defaultinit]") {
  withDefaultInitOverDirtyMemory<Dirent>([](Dirent& d) {
    REQUIRE(d.Id == Zero32);
    REQUIRE(d.MetaId == Zero32);
    REQUIRE(d.ParentId == Zero32);
  });
}

TEST_CASE("Inode default-init zeros Id", "[defaultinit]") {
  withDefaultInitOverDirtyMemory<Inode>([](Inode& d) {
    REQUIRE(d.Id == Zero32);
  });
}

TEST_CASE("Extent default-init zeros InodeId", "[defaultinit]") {
  withDefaultInitOverDirtyMemory<Extent>([](Extent& d) {
    REQUIRE(d.InodeId == Zero32);
  });
}

TEST_CASE("Entry default-init zeros InodeId", "[defaultinit]") {
  // Entry has user-provided constructors, so default-init goes through one of
  // them; the constructor doesn't touch InodeId, so absence of an in-class
  // initializer still leaks bytes.
  alignas(Entry) std::byte buf[sizeof(Entry)];
  std::memset(buf, 0xCC, sizeof(buf));
  Entry* p = ::new (static_cast<void*>(buf)) Entry(0);
  REQUIRE(p->InodeId == Zero32);
  p->~Entry();
}

TEST_CASE("FilesystemRec default-init zeros RootDirentId and RootInodeId", "[defaultinit]") {
  withDefaultInitOverDirtyMemory<FilesystemRec>([](FilesystemRec& d) {
    REQUIRE(d.RootDirentId == Zero32);
    REQUIRE(d.RootInodeId == Zero32);
  });
}

TEST_CASE("RuleMatch default-init zeros inode_id", "[defaultinit]") {
  withDefaultInitOverDirtyMemory<RuleMatch>([](RuleMatch& d) {
    REQUIRE(d.inode_id == Zero32);
  });
}

TEST_CASE("SearchHit default-init zeros file_hash", "[defaultinit]") {
  withDefaultInitOverDirtyMemory<SearchHit>([](SearchHit& d) {
    REQUIRE(d.file_hash == Zero32);
  });
}

TEST_CASE("FileSigResult default-init zeros FileHash", "[defaultinit]") {
  withDefaultInitOverDirtyMemory<FileSigResult>([](FileSigResult& d) {
    REQUIRE(d.FileHash == Zero32);
  });
}

TEST_CASE("HashRec default-init zeros InodeId", "[defaultinit]") {
  withDefaultInitOverDirtyMemory<HashRec>([](HashRec& d) {
    REQUIRE(d.InodeId == Zero32);
  });
}
