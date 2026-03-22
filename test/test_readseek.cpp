#include <catch2/catch_test_macros.hpp>

#include <cstring>

#include "readseek_impl.h"
#include "tsk.h"

namespace {
  std::vector<uint8_t> SRCBUF{ 35, 32, 113, 65 };

  ReadSeekBuf makeBufRS() {
    return ReadSeekBuf(SRCBUF);
  }

  struct TSKTestFixture {
    std::unique_ptr<TSK_IMG_INFO, decltype(&tsk_img_close)> img;
    std::shared_ptr<TSK_FS_INFO> fs;

    TSKTestFixture():
      img(nullptr, tsk_img_close)
    {
      const char* path = "test/data/ntfs-img-kw-1.dd";
      img.reset(tsk_img_open_utf8(1, &path, TSK_IMG_TYPE_DETECT, 0));
      if (img) {
        fs = std::shared_ptr<TSK_FS_INFO>(
          tsk_fs_open_img(img.get(), 0, TSK_FS_TYPE_DETECT),
          tsk_fs_close
        );
      }
    }

    ReadSeekTSK makeRS(uint64_t inum) {
      return ReadSeekTSK(fs, inum);
    }
  };

  const uint64_t TSK_TEST_INUM = 33;  // file-n-1.dat
  const size_t TSK_TEST_SIZE = 2000;

  ReadSeekFile makeFileRS(std::shared_ptr<FILE>& f) {
    f.reset(std::tmpfile(), std::fclose);
    for (uint8_t b: SRCBUF) {
      std::fputc(b, f.get());
    }
    std::fseek(f.get(), 0, SEEK_SET);
    return ReadSeekFile(f);
  }
}

// --- Contract test functions ---
// Each tests one property of the ReadSeek contract.
// They assume the ReadSeek is open and at position 0.
// Content-independent: they read actual data and compare against itself,
// not against compile-time constants, so they work for any implementation.

void testInitialState(ReadSeek& rs, size_t expectedSize) {
  REQUIRE(rs.tellg() == 0);
  REQUIRE(rs.size() == expectedSize);
}

void testReadFullVector(ReadSeek& rs) {
  size_t sz = rs.size();
  std::vector<uint8_t> buf;
  REQUIRE(rs.read(sz, buf) == sz);
  REQUIRE(buf.size() == sz);
  REQUIRE(rs.tellg() == rs.size());
}

void testReadFullRawPtr(ReadSeek& rs) {
  size_t sz = rs.size();
  std::vector<uint8_t> raw(sz);
  REQUIRE(rs.read(sz, raw.data()) == sz);
  REQUIRE(rs.tellg() == rs.size());
}

void testReadAtEOFReturnsZero(ReadSeek& rs) {
  std::vector<uint8_t> buf;
  rs.seek(rs.size());
  REQUIRE(rs.read(1, buf) == 0);
  REQUIRE(rs.tellg() == rs.size());
}

void testReadPastEOFRawPtrReturnsZero(ReadSeek& rs) {
  uint8_t raw;
  rs.seek(rs.size());
  REQUIRE(rs.read(1, &raw) == 0);
  REQUIRE(rs.tellg() == rs.size());
}

void testPartialReadAtEndRawPtr(ReadSeek& rs) {
  // Read full content to get the last byte
  rs.seek(0);
  std::vector<uint8_t> full(rs.size());
  rs.read(rs.size(), full.data());
  uint8_t lastByte = full.back();

  rs.seek(rs.size() - 1);
  uint8_t raw[2] = {};
  REQUIRE(rs.read(100, raw) == 1);
  REQUIRE(raw[0] == lastByte);
  REQUIRE(rs.tellg() == rs.size());
}

void testSeekAndRead(ReadSeek& rs) {
  // Read bytes at positions 1-2 via full read, then via seek+read
  rs.seek(0);
  std::vector<uint8_t> full(rs.size());
  rs.read(rs.size(), full.data());

  rs.seek(1);
  REQUIRE(rs.tellg() == 1);
  std::vector<uint8_t> buf;
  REQUIRE(rs.read(2, buf) == 2);
  REQUIRE(buf[0] == full[1]);
  REQUIRE(buf[1] == full[2]);
  REQUIRE(rs.tellg() == 3);
}

void testSeekPastEndClampsToSize(ReadSeek& rs) {
  REQUIRE(rs.seek(rs.size() + 100) == rs.size());
  REQUIRE(rs.tellg() == rs.size());
}

void testZeroLengthReadVector(ReadSeek& rs) {
  std::vector<uint8_t> buf;
  REQUIRE(rs.read(0, buf) == 0);
  REQUIRE(rs.tellg() == 0);
}

void testZeroLengthReadRawPtr(ReadSeek& rs) {
  uint8_t raw;
  REQUIRE(rs.read(0, &raw) == 0);
  REQUIRE(rs.tellg() == 0);
}

void testRewindAndReread(ReadSeek& rs) {
  size_t sz = rs.size();
  std::vector<uint8_t> buf1(sz), buf2(sz);
  rs.read(sz, buf1.data());
  REQUIRE(rs.tellg() == rs.size());
  rs.seek(0);
  REQUIRE(rs.tellg() == 0);
  rs.read(sz, buf2.data());
  REQUIRE(buf1 == buf2);
}

void testPartialReadAtEnd(ReadSeek& rs) {
  // Read full content to get the last byte
  rs.seek(0);
  std::vector<uint8_t> full(rs.size());
  rs.read(rs.size(), full.data());
  uint8_t lastByte = full.back();

  rs.seek(rs.size() - 1);
  std::vector<uint8_t> buf;
  REQUIRE(rs.read(100, buf) == 1);
  REQUIRE(buf.size() == 1);
  REQUIRE(buf[0] == lastByte);
  REQUIRE(rs.tellg() == rs.size());
}

void testChunkedReadMatchesFullContent(ReadSeek& rs) {
  // Read full content for comparison
  rs.seek(0);
  std::vector<uint8_t> fullContent(rs.size());
  rs.read(rs.size(), fullContent.data());

  // Rewind and read in chunks via vector overload
  rs.seek(0);
  std::vector<uint8_t> buf;
  std::vector<uint8_t> accumulated;
  while (true) {
    size_t n = rs.read(2, buf);
    if (n == 0) break;
    REQUIRE(buf.size() == n);
    accumulated.insert(accumulated.end(), buf.begin(), buf.end());
  }
  REQUIRE(accumulated == fullContent);
  REQUIRE(rs.tellg() == rs.size());
}

void testVectorAndRawPtrReturnSameData(ReadSeek& rs) {
  size_t sz = rs.size();
  std::vector<uint8_t> vecBuf;
  rs.read(sz, vecBuf);

  rs.seek(0);
  std::vector<uint8_t> rawBuf(sz);
  rs.read(sz, rawBuf.data());

  REQUIRE(vecBuf == rawBuf);
}

// --- ReadSeekBuf TEST_CASEs ---

TEST_CASE("readSeekBuf_initialState") {
  auto rs = makeBufRS();
  testInitialState(rs, SRCBUF.size());
}

TEST_CASE("readSeekBuf_readFullVector") {
  auto rs = makeBufRS();
  testReadFullVector(rs);
}

TEST_CASE("readSeekBuf_readFullRawPtr") {
  auto rs = makeBufRS();
  testReadFullRawPtr(rs);
}

TEST_CASE("readSeekBuf_readAtEOFReturnsZero") {
  auto rs = makeBufRS();
  testReadAtEOFReturnsZero(rs);
}

TEST_CASE("readSeekBuf_readPastEOFRawPtrReturnsZero") {
  auto rs = makeBufRS();
  testReadPastEOFRawPtrReturnsZero(rs);
}

TEST_CASE("readSeekBuf_partialReadAtEndRawPtr") {
  auto rs = makeBufRS();
  testPartialReadAtEndRawPtr(rs);
}

TEST_CASE("readSeekBuf_seekAndRead") {
  auto rs = makeBufRS();
  testSeekAndRead(rs);
}

TEST_CASE("readSeekBuf_seekPastEndClampsToSize") {
  auto rs = makeBufRS();
  testSeekPastEndClampsToSize(rs);
}

TEST_CASE("readSeekBuf_zeroLengthReadVector") {
  auto rs = makeBufRS();
  testZeroLengthReadVector(rs);
}

TEST_CASE("readSeekBuf_zeroLengthReadRawPtr") {
  auto rs = makeBufRS();
  testZeroLengthReadRawPtr(rs);
}

TEST_CASE("readSeekBuf_rewindAndReread") {
  auto rs = makeBufRS();
  testRewindAndReread(rs);
}

TEST_CASE("readSeekBuf_vectorAndRawPtrReturnSameData") {
  auto rs = makeBufRS();
  testVectorAndRawPtrReturnSameData(rs);
}

TEST_CASE("readSeekBuf_partialReadAtEnd") {
  auto rs = makeBufRS();
  testPartialReadAtEnd(rs);
}

TEST_CASE("readSeekBuf_chunkedReadMatchesFullContent") {
  auto rs = makeBufRS();
  testChunkedReadMatchesFullContent(rs);
}

TEST_CASE("readSeekBuf_emptyBuffer") {
  std::vector<uint8_t> empty;
  ReadSeekBuf rs(empty);
  REQUIRE(rs.size() == 0);
  REQUIRE(rs.tellg() == 0);

  std::vector<uint8_t> buf;
  REQUIRE(rs.read(1, buf) == 0);
  REQUIRE(rs.tellg() == 0);

  REQUIRE(rs.seek(10) == 0);
  REQUIRE(rs.tellg() == 0);

  uint8_t raw;
  REQUIRE(rs.read(1, &raw) == 0);
}

// --- ReadSeekFile TEST_CASEs ---

TEST_CASE("readSeekFile_initialState") {
  std::shared_ptr<FILE> f;
  auto rs = makeFileRS(f);
  testInitialState(rs, SRCBUF.size());
}

TEST_CASE("readSeekFile_readFullVector") {
  std::shared_ptr<FILE> f;
  auto rs = makeFileRS(f);
  testReadFullVector(rs);
}

TEST_CASE("readSeekFile_readFullRawPtr") {
  std::shared_ptr<FILE> f;
  auto rs = makeFileRS(f);
  testReadFullRawPtr(rs);
}

TEST_CASE("readSeekFile_readAtEOFReturnsZero") {
  std::shared_ptr<FILE> f;
  auto rs = makeFileRS(f);
  testReadAtEOFReturnsZero(rs);
}

TEST_CASE("readSeekFile_readPastEOFRawPtrReturnsZero") {
  std::shared_ptr<FILE> f;
  auto rs = makeFileRS(f);
  testReadPastEOFRawPtrReturnsZero(rs);
}

TEST_CASE("readSeekFile_partialReadAtEndRawPtr") {
  std::shared_ptr<FILE> f;
  auto rs = makeFileRS(f);
  testPartialReadAtEndRawPtr(rs);
}

TEST_CASE("readSeekFile_seekAndRead") {
  std::shared_ptr<FILE> f;
  auto rs = makeFileRS(f);
  testSeekAndRead(rs);
}

TEST_CASE("readSeekFile_seekPastEndClampsToSize") {
  std::shared_ptr<FILE> f;
  auto rs = makeFileRS(f);
  testSeekPastEndClampsToSize(rs);
}

TEST_CASE("readSeekFile_zeroLengthReadVector") {
  std::shared_ptr<FILE> f;
  auto rs = makeFileRS(f);
  testZeroLengthReadVector(rs);
}

TEST_CASE("readSeekFile_zeroLengthReadRawPtr") {
  std::shared_ptr<FILE> f;
  auto rs = makeFileRS(f);
  testZeroLengthReadRawPtr(rs);
}

TEST_CASE("readSeekFile_rewindAndReread") {
  std::shared_ptr<FILE> f;
  auto rs = makeFileRS(f);
  testRewindAndReread(rs);
}

TEST_CASE("readSeekFile_vectorAndRawPtrReturnSameData") {
  std::shared_ptr<FILE> f;
  auto rs = makeFileRS(f);
  testVectorAndRawPtrReturnSameData(rs);
}

TEST_CASE("readSeekFile_partialReadAtEnd") {
  std::shared_ptr<FILE> f;
  auto rs = makeFileRS(f);
  testPartialReadAtEnd(rs);
}

TEST_CASE("readSeekFile_chunkedReadMatchesFullContent") {
  std::shared_ptr<FILE> f;
  auto rs = makeFileRS(f);
  testChunkedReadMatchesFullContent(rs);
}

// --- ReadSeekTSK TEST_CASEs ---

TEST_CASE("readSeekTSK_initialState") {
  TSKTestFixture fix;
  REQUIRE(fix.fs);
  auto rs = fix.makeRS(TSK_TEST_INUM);
  REQUIRE(rs.open());
  testInitialState(rs, TSK_TEST_SIZE);
  rs.close();
}

TEST_CASE("readSeekTSK_readFullVectorChecksTellg") {
  TSKTestFixture fix;
  REQUIRE(fix.fs);
  auto rs = fix.makeRS(TSK_TEST_INUM);
  REQUIRE(rs.open());
  std::vector<uint8_t> buf;
  REQUIRE(rs.read(TSK_TEST_SIZE, buf) == TSK_TEST_SIZE);
  REQUIRE(buf.size() == TSK_TEST_SIZE);
  REQUIRE(buf[0] == 0xb1);
  REQUIRE(buf[1] == 0x51);
  REQUIRE(buf[2] == 0xb4);
  REQUIRE(buf[3] == 0x77);
  REQUIRE(rs.tellg() == rs.size());  // This will FAIL — tellg() returns 0
  rs.close();
}
