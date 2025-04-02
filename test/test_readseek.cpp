#include <catch2/catch_test_macros.hpp>

#include "readseek_impl.h"
#include "llama.h"

namespace {
  std::vector<uint8_t> SRCBUF{ 35, 32, 113, 65 };
}

void basicReadSeekTest(const std::vector<uint8_t>& srcbuf, ReadSeek& rs) {
  std::vector<uint8_t> buf;
  std::unique_ptr<uint8_t[]> bufPtr(new uint8_t[srcbuf.size()]);

  REQUIRE(rs.tellg() == 0);
  REQUIRE(rs.size() == srcbuf.size());
  REQUIRE(size_t(rs.read(srcbuf.size(), buf)) == srcbuf.size());
  REQUIRE(buf == srcbuf);
  REQUIRE(rs.tellg() == rs.size());
  REQUIRE(rs.read(1, buf) == 0); // read past end should return 0

  REQUIRE(rs.seek(1) == 1);
  REQUIRE(rs.tellg() == 1);
  REQUIRE(rs.read(2, buf) == 2);
  REQUIRE(buf == std::vector<uint8_t>{32, 113});
  REQUIRE(rs.tellg() == 3);

  REQUIRE(rs.seek(rs.size() + 1) == rs.size());

  REQUIRE(rs.seek(0) == 0);
  REQUIRE(size_t(rs.read(srcbuf.size(), bufPtr.get())) == srcbuf.size());
  REQUIRE(rs.tellg() == rs.size());
  REQUIRE(size_t(rs.read(1, bufPtr.get())) == 0);
  REQUIRE(rs.seek(1) == 1);
  REQUIRE(rs.tellg() == 1);
  REQUIRE(rs.read(2, bufPtr.get()) == 2);
  REQUIRE(*bufPtr.get() == 32);
  REQUIRE(*(bufPtr.get() + 1) == 113);
  REQUIRE(rs.tellg() == 3);
}

TEST_CASE("readSeekBuf") {
  ReadSeekBuf rs(SRCBUF);
  basicReadSeekTest(SRCBUF, rs);
}

TEST_CASE("readSeekFile") {
  std::shared_ptr<FILE> f(std::tmpfile(), std::fclose);
  REQUIRE(f);

  for (uint8_t b: SRCBUF) {
    std::fputc(b, f.get());
  }
  std::fseek(f.get(), 0, SEEK_SET);

  ReadSeekFile rs(f);
  basicReadSeekTest(SRCBUF, rs);
}
