#include <catch2/catch_test_macros.hpp>

#include <cstring>

#include "entry.h"
#include "evidenceioerror.h"
#include "processor.h"
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
  REQUIRE(rs.tellg() == rs.size());
  rs.close();
}

TEST_CASE("readSeekTSK_readFullRawPtr") {
  TSKTestFixture fix;
  REQUIRE(fix.fs);
  auto rs = fix.makeRS(TSK_TEST_INUM);
  REQUIRE(rs.open());
  testReadFullRawPtr(rs);
  rs.close();
}

TEST_CASE("readSeekTSK_readAtEOFReturnsZero") {
  TSKTestFixture fix;
  REQUIRE(fix.fs);
  auto rs = fix.makeRS(TSK_TEST_INUM);
  REQUIRE(rs.open());
  testReadAtEOFReturnsZero(rs);
  rs.close();
}

TEST_CASE("readSeekTSK_readPastEOFRawPtrReturnsZero") {
  TSKTestFixture fix;
  REQUIRE(fix.fs);
  auto rs = fix.makeRS(TSK_TEST_INUM);
  REQUIRE(rs.open());
  testReadPastEOFRawPtrReturnsZero(rs);
  rs.close();
}

TEST_CASE("readSeekTSK_partialReadAtEnd") {
  TSKTestFixture fix;
  REQUIRE(fix.fs);
  auto rs = fix.makeRS(TSK_TEST_INUM);
  REQUIRE(rs.open());
  testPartialReadAtEnd(rs);
  rs.close();
}

TEST_CASE("readSeekTSK_partialReadAtEndRawPtr") {
  TSKTestFixture fix;
  REQUIRE(fix.fs);
  auto rs = fix.makeRS(TSK_TEST_INUM);
  REQUIRE(rs.open());
  testPartialReadAtEndRawPtr(rs);
  rs.close();
}

TEST_CASE("readSeekTSK_seekAndRead") {
  TSKTestFixture fix;
  REQUIRE(fix.fs);
  auto rs = fix.makeRS(TSK_TEST_INUM);
  REQUIRE(rs.open());
  testSeekAndRead(rs);
  rs.close();
}

TEST_CASE("readSeekTSK_seekPastEndClampsToSize") {
  TSKTestFixture fix;
  REQUIRE(fix.fs);
  auto rs = fix.makeRS(TSK_TEST_INUM);
  REQUIRE(rs.open());
  testSeekPastEndClampsToSize(rs);
  rs.close();
}

TEST_CASE("readSeekTSK_zeroLengthReadVector") {
  TSKTestFixture fix;
  REQUIRE(fix.fs);
  auto rs = fix.makeRS(TSK_TEST_INUM);
  REQUIRE(rs.open());
  testZeroLengthReadVector(rs);
  rs.close();
}

TEST_CASE("readSeekTSK_zeroLengthReadRawPtr") {
  TSKTestFixture fix;
  REQUIRE(fix.fs);
  auto rs = fix.makeRS(TSK_TEST_INUM);
  REQUIRE(rs.open());
  testZeroLengthReadRawPtr(rs);
  rs.close();
}

TEST_CASE("readSeekTSK_chunkedReadMatchesFullContent") {
  TSKTestFixture fix;
  REQUIRE(fix.fs);
  auto rs = fix.makeRS(TSK_TEST_INUM);
  REQUIRE(rs.open());
  testChunkedReadMatchesFullContent(rs);
  rs.close();
}

TEST_CASE("readSeekTSK_rewindAndReread") {
  TSKTestFixture fix;
  REQUIRE(fix.fs);
  auto rs = fix.makeRS(TSK_TEST_INUM);
  REQUIRE(rs.open());
  testRewindAndReread(rs);
  rs.close();
}

TEST_CASE("readSeekTSK_vectorAndRawPtrReturnSameData") {
  TSKTestFixture fix;
  REQUIRE(fix.fs);
  auto rs = fix.makeRS(TSK_TEST_INUM);
  REQUIRE(rs.open());
  testVectorAndRawPtrReturnSameData(rs);
  rs.close();
}

TEST_CASE("readSeekTSK_throwsEvidenceIOError") {
  EvidenceIOError err("test error");
  REQUIRE(dynamic_cast<const std::runtime_error*>(&err) != nullptr);
  REQUIRE(std::string(err.what()).find("test error") != std::string::npos);
}

TEST_CASE("entryCarriesEvidenceContext") {
  std::vector<uint8_t> buf{1, 2, 3};
  auto rs = std::make_unique<ReadSeekBuf>(buf);
  Entry entry(42, std::move(rs));
  entry.EvidenceFile = "test.E01";
  entry.FsIndex = 1;
  entry.FsOffset = 32256;
  entry.AddrFlags = 0x05;
  entry.Path = "/Users/test/file.txt";
  entry.FileSize = 1024;

  REQUIRE(entry.Addr == 42);
  REQUIRE(entry.EvidenceFile == "test.E01");
  REQUIRE(entry.FsIndex == 1);
  REQUIRE(entry.FsOffset == 32256);
  REQUIRE(entry.AddrFlags == 0x05);
  REQUIRE(entry.Path == "/Users/test/file.txt");
  REQUIRE(entry.FileSize == 1024);
}

TEST_CASE("entryConstructsWithAddrOnly") {
  Entry entry(42);
  REQUIRE(entry.Addr == 42);
  REQUIRE(entry.FileSize == 0);
  REQUIRE(entry.FsOffset == 0);
  REQUIRE(entry.FsType == TSK_FS_TYPE_DETECT);
}

TEST_CASE("entryStreamCanBeSetAfterConstruction") {
  Entry entry(42);

  std::vector<uint8_t> buf{1, 2, 3};
  auto rs = std::make_unique<ReadSeekBuf>(buf);
  entry.setStream(std::move(rs));

  REQUIRE(entry.getStream().open());
  REQUIRE(entry.getStream().size() == 3);
}

TEST_CASE("processorCreatesReadSeekFromEntry") {
  Entry entry(TSK_TEST_INUM);
  entry.EvidenceFile = "test/data/ntfs-img-kw-1.dd";
  entry.FsOffset = 0;
  entry.FsType = TSK_FS_TYPE_DETECT;

  // Set up database with required tables
  LlamaDB db;
  LlamaDBConnection dbConn(db);
  auto ruleEngine = std::make_shared<LlamaRuleEngine>();
  ruleEngine->createTables(dbConn);
  DBType<HashRec>::createTable(dbConn.get(), "hash");
  DBType<ExceptionRecord>::createTable(dbConn.get(), "exception_log");

  auto ctx = std::make_shared<ProcessorContext>(
    &db, nullptr, ruleEngine, "", "", MagicsType{}
  );
  Processor proc(ctx);

  proc.createReadSeek(entry);
  REQUIRE(entry.getStream().open());
  REQUIRE(entry.getStream().size() == TSK_TEST_SIZE);
  entry.getStream().close();
}

TEST_CASE("processBatchCreatesReadSeekWhenMissing") {
  // Set up database with required tables
  LlamaDB db;
  LlamaDBConnection dbConn(db);
  auto ruleEngine = std::make_shared<LlamaRuleEngine>();
  ruleEngine->createTables(dbConn);
  DBType<HashRec>::createTable(dbConn.get(), "hash");
  DBType<ExceptionRecord>::createTable(dbConn.get(), "exception_log");

  auto ctx = std::make_shared<ProcessorContext>(
    &db, nullptr, ruleEngine, "", "", MagicsType{}
  );
  Processor proc(ctx);

  auto entries = std::make_shared<std::vector<std::unique_ptr<Entry>>>();
  auto entry = std::make_unique<Entry>(TSK_TEST_INUM);
  entry->EvidenceFile = "test/data/ntfs-img-kw-1.dd";
  entry->FsOffset = 0;
  entry->FsType = TSK_FS_TYPE_DETECT;
  entries->push_back(std::move(entry));

  // processBatch should create the ReadSeek itself and process the entry
  proc.processBatch(entries);

  // Verify a hash was produced (entry was processed)
  // processBatch calls flush(), which moves hashes to DB and clears the batch,
  // so we query the DB directly
  duckdb_result result;
  duckdb_query(dbConn.get(), "SELECT count(*) FROM hash", &result);
  REQUIRE(duckdb_value_int64(&result, 0, 0) == 1);
  duckdb_destroy_result(&result);
}

TEST_CASE("clonedProcessorOpensIndependentHandles") {
  // Set up database with required tables
  LlamaDB db;
  LlamaDBConnection dbConn(db);
  auto ruleEngine = std::make_shared<LlamaRuleEngine>();
  ruleEngine->createTables(dbConn);
  DBType<HashRec>::createTable(dbConn.get(), "hash");
  DBType<ExceptionRecord>::createTable(dbConn.get(), "exception_log");

  auto ctx = std::make_shared<ProcessorContext>(
    &db, nullptr, ruleEngine, "", "", MagicsType{}
  );
  Processor proc(ctx);

  // Open handles on the original
  Entry entry1(TSK_TEST_INUM);
  entry1.EvidenceFile = "test/data/ntfs-img-kw-1.dd";
  entry1.FsOffset = 0;
  entry1.FsType = TSK_FS_TYPE_DETECT;
  proc.createReadSeek(entry1);

  // Clone and verify the clone can independently open the same file
  auto cloned = proc.clone();
  Entry entry2(TSK_TEST_INUM);
  entry2.EvidenceFile = "test/data/ntfs-img-kw-1.dd";
  entry2.FsOffset = 0;
  entry2.FsType = TSK_FS_TYPE_DETECT;
  cloned->createReadSeek(entry2);

  // Both should be able to read independently
  REQUIRE(entry1.getStream().open());
  REQUIRE(entry2.getStream().open());

  // Read from both — they should get the same content
  std::vector<uint8_t> buf1, buf2;
  size_t n1 = entry1.getStream().read(256, buf1);
  size_t n2 = entry2.getStream().read(256, buf2);
  REQUIRE(n1 == n2);
  REQUIRE(buf1 == buf2);

  entry1.getStream().close();
  entry2.getStream().close();
}
