#include <catch2/catch_test_macros.hpp>

#include "inodeandblocktrackerimpl.h"

#include <stdexcept>

TEST_CASE("testInodeVisit") {
  InodeAndBlockTrackerImpl t;
  t.setInodeRange(0, 256);

  // none are seen yet
  for (int i = 0; i < 256; ++i) {
    REQUIRE(!t.markInodeSeen(i));
  }

  // all are seen now
  for (int i = 0; i < 256; ++i) {
    REQUIRE(t.markInodeSeen(i));
  }

  // reset (doesn't matter if its the same size)
  t.setInodeRange(0, 256);
  
  // none are seen once again
  for (int i = 0; i < 256; ++i) {
    REQUIRE(!t.markInodeSeen(i));
  }
}

TEST_CASE("testInodeBadRange") {
  InodeAndBlockTrackerImpl t;
  CHECK_THROWS_AS(t.setInodeRange(3, 1), std::runtime_error);
}

TEST_CASE("testInodeTooSmall") {
  InodeAndBlockTrackerImpl t;
  t.setInodeRange(1, 3);
  CHECK_THROWS_AS(t.markInodeSeen(0), std::runtime_error);
}

TEST_CASE("testInodeTooLarge") {
  InodeAndBlockTrackerImpl t;
  t.setInodeRange(1, 3);
  CHECK_THROWS_AS(t.markInodeSeen(3), std::runtime_error);
}

// TEST_CASE("testBlocksAllocated") {
//   InodeAndBlockTrackerImpl t;
//   t.setBlockRange(0, 256);
// }
