#include <catch2/catch_test_macros.hpp>

#include "processor.h"

#include "duckexception.h"
#include "entry.h"
#include "ducksig.h"
#include "filesignatures.h"
#include "lightgrep/api.h"
#include "filerecord.h"
#include "mockoutputhandler.h"
#include "readseek_impl.h"
#include "patternparser.h"

#include <hasher/api.h>

#include <iostream>
#include <fstream>
#include <thread>
#include <vector>

#include "boost_asio.h"

TEST_CASE("testBoostThreadPool") {
  unsigned int count = 0;
  boost::asio::thread_pool pool(2);
  boost::asio::post(pool, [&]() {
    ++count;
    boost::asio::post(pool, [&]() {
      ++count;
    });
  });
  pool.join();
  REQUIRE(2u == count);
}

class ProcessorSearchTester {
public:
  ProcessorSearchTester(std::string needle, std::string haystack, uint64_t numExpectedHits) 
  : RuleEngine(new LlamaRuleEngine()), RsBuf(haystack), Db(), DbConn(Db), Proc(createProcessor(needle)) {
    Proc.setSHA256("file_hash");
    RuleEngine->setPatternToRuleId(std::vector<std::string>(numExpectedHits, "rule_id"));
  }

  void search() {
    Proc.search(RsBuf);
  }

  uint64_t putSearchHitsInDb() {
    LlamaDBAppender appender(DbConn.get(), "search_hits");
    uint64_t recordsInserted = Proc.searchHits()->copyToDB(appender.get());
    appender.flush();
    return recordsInserted;
  }

  uint64_t createTempTableAndPopulate(const std::vector<SearchHit>& expectedSearchHits) {
    DBType<SearchHit>::createTable(DbConn.get(), "temp_search_hits");
    LlamaDBAppender tempAppender(DbConn.get(), "temp_search_hits");
    DBBatch<SearchHit> tempSearchHits;
    for (const SearchHit& sh : expectedSearchHits) {
      tempSearchHits.add(sh);
    }
    uint64_t recordsInserted = tempSearchHits.copyToDB(tempAppender.get());
    tempAppender.flush();
    return recordsInserted;
  }

  uint64_t numDiffsBetweenTables() {
    duckdb_result result;
    std::string diffUnionQuery = "(Select * from search_hits Except Select * from temp_search_hits ) UNION ALL (Select * from search_hits Except Select * from temp_search_hits)";
    duckdb_query(DbConn.get(), (diffUnionQuery + ";").c_str(), &result);
    auto diffs = duckdb_row_count(&result);
    duckdb_destroy_result(&result);
    return diffs;
  }

private:
  Processor createProcessor(std::string needle) {
    RuleEngine->createTables(DbConn);
    std::shared_ptr<PatternHandle> pat(lg_create_pattern(), lg_destroy_pattern);
    LG_KeyOptions opts{0,0,0};
    LG_Error* err(nullptr);
    lg_parse_pattern(pat.get(), needle.c_str(), &opts, &err);
    std::shared_ptr<FSMHandle> fsm(lg_create_fsm(0, 0), lg_destroy_fsm);
    lg_add_pattern(fsm.get(), pat.get(), "ASCII", 0, &err);
    LG_ProgramOptions pOpts{10};
    LG_HPROGRAM prog(lg_create_program(fsm.get(), &pOpts));
    std::shared_ptr<ProgramHandle> pHandle(prog, lg_destroy_program);

    // duckdb setup
    DBType<SearchHit>::createTable(DbConn.get(), "search_hits");
    DBType<HashRec>::createTable(DbConn.get(), "hash");
    DBType<FileSigResult>::createTable(DbConn.get(), "file_signatures");
    DBType<ExceptionRecord>::createTable(DbConn.get(), "exception_log");

    auto procContext = std::make_shared<ProcessorContext>(&Db, pHandle, RuleEngine, "", "", MagicsType{});
    return Processor(procContext);
  }
  std::shared_ptr<LlamaRuleEngine> RuleEngine;
  ReadSeekBuf RsBuf;
  LlamaDB Db;
  LlamaDBConnection DbConn;
  Processor Proc;
};

TEST_CASE("testBasicOneHitSearch") {
  std::string needle = "foobar";
  std::string haystack = "this is so foobar";

  std::vector<SearchHit> expectedHits = {
    SearchHit{"foobar", 11, 17, "rule_id", "file_hash", 6}
  };

  ProcessorSearchTester pst{needle, haystack, expectedHits.size()};
  pst.search();

  REQUIRE(expectedHits.size() == pst.putSearchHitsInDb());
  pst.createTempTableAndPopulate(expectedHits);
  REQUIRE(0 == pst.numDiffsBetweenTables());
}

TEST_CASE("testSearchThatSpansMultipleBuffers") {
  std::string needle = "a+";

  // arbitrary length over the buffer size
  uint64_t hitLength = (1 << 20) + 4;

  std::vector<char> v(hitLength, 'a');
  std::string haystack(v.begin(), v.end());
  CHECK(hitLength == haystack.size());

  std::vector<SearchHit> expectedHits = {
    SearchHit{needle, 0, hitLength, "rule_id", "file_hash", hitLength}
  };

  ProcessorSearchTester pst{needle, haystack, expectedHits.size()};
  pst.search();

  REQUIRE(expectedHits.size() == pst.putSearchHitsInDb());
  pst.createTempTableAndPopulate(expectedHits);
  REQUIRE(0 == pst.numDiffsBetweenTables());
}

TEST_CASE("testSearchWithMultipleHits") {
  std::string needle = "foo";
  std::string haystack = "foo is foobar is foobaz";

  std::vector<SearchHit> expectedHits{
    SearchHit{"foo", 0, 3, "rule_id", "file_hash", 3},
    SearchHit{"foo", 7, 10, "rule_id", "file_hash", 3},
    SearchHit{"foo", 17, 20, "rule_id", "file_hash", 3},
  };

  ProcessorSearchTester pst(needle, haystack, expectedHits.size());
  pst.search();

  REQUIRE(expectedHits.size() == pst.putSearchHitsInDb());
  pst.createTempTableAndPopulate(expectedHits);
  REQUIRE(0 == pst.numDiffsBetweenTables());

}

TEST_CASE("testProcessorContextGetSupportedHashAlgsDiffAlgs") {
  std::shared_ptr<LlamaRuleEngine> ruleEngine = std::make_shared<LlamaRuleEngine>();
  ProcessorContext procCtx{
    nullptr,
    nullptr,
    ruleEngine,
    "test/hsets/md5.hset",
    "test/hsets/sha1.hset",
    MagicsType{}
  };

  REQUIRE(procCtx.getSupportedHashAlgsFromContext() == (SFHASH_SHA_2_256 | SFHASH_MD5 | SFHASH_SHA_1));
}

TEST_CASE("testProcessorContextGetSupportedHashAlgsSameAlgs") {
  std::shared_ptr<LlamaRuleEngine> ruleEngine = std::make_shared<LlamaRuleEngine>();
  ProcessorContext procCtx{
    nullptr,
    nullptr,
    ruleEngine,
    "test/hsets/md5.hset",
    "test/hsets/md5.hset",
    MagicsType{}
  };

  REQUIRE(procCtx.getSupportedHashAlgsFromContext() == (SFHASH_SHA_2_256 | SFHASH_MD5));
}

TEST_CASE("testProcessorContextGetSupportedHashAlgsMultipleAlgs") {
  // When there are multiple algs available in the hashset, the first responsive one is
  // dependent on the order of the algs in hashset<anonymous namespace>::searchedHashAlgs.
  // In this case, it's MD5 because MD5 comes first in searchedHashAlgs.
  std::shared_ptr<LlamaRuleEngine> ruleEngine = std::make_shared<LlamaRuleEngine>();
  ProcessorContext procCtx{
    nullptr,
    nullptr,
    ruleEngine,
    "test/hsets/md5.hset",
    "test/hsets/sha1_md5.hset",
    MagicsType{}
  };

  REQUIRE(procCtx.getSupportedHashAlgsFromContext() == (SFHASH_SHA_2_256 | SFHASH_MD5));
}

TEST_CASE("Processor::flush clears batches to prevent duplicates") {
  // When a Processor is reused from the FileScheduler pool, flush() must
  // clear its batches. Otherwise each subsequent flush re-writes all
  // previously accumulated records, producing duplicates.

  LlamaDB db;
  LlamaDBConnection conn(db);

  auto ruleEngine = std::make_shared<LlamaRuleEngine>();
  ruleEngine->createTables(conn);
  DBType<HashRec>::createTable(conn.get(), "hash");
  DBType<SearchHit>::createTable(conn.get(), "search_hits");
  DBType<FileSigResult>::createTable(conn.get(), "file_signatures");
  DBType<ExceptionRecord>::createTable(conn.get(), "exception_log");

  auto procContext = std::make_shared<ProcessorContext>(
    &db, nullptr, ruleEngine, "", "",
    MagicsType{}
  );
  Processor proc(procContext);

  // First batch: add 2 hash records and flush
  proc.hashBatch()->add(HashRec{1, "md5_1", "sha1_1", "sha256_1", "blake3_1", "ssdeep_1"});
  proc.hashBatch()->add(HashRec{2, "md5_2", "sha1_2", "sha256_2", "blake3_2", "ssdeep_2"});
  proc.flush();

  // Second batch: add 1 more hash record and flush (same Processor, reused)
  proc.hashBatch()->add(HashRec{3, "md5_3", "sha1_3", "sha256_3", "blake3_3", "ssdeep_3"});
  proc.flush();

  // Query the database for total hash row count
  duckdb_result result;
  duckdb_query(conn.get(), "SELECT count(*) FROM hash", &result);
  auto rowCount = duckdb_value_int64(&result, 0, 0);
  duckdb_destroy_result(&result);

  // Without clear() in flush(), the second flush re-writes all 3 accumulated
  // rows, giving 5 total (2 from first flush + 3 from second).
  // Correct behavior: exactly 3 rows.
  REQUIRE(rowCount == 3);
}

TEST_CASE("ProcessorContext can be constructed with SigMagics") {
  // Load magics
  std::shared_ptr<FILE> magicsFile(std::fopen("./magics.json", "rb"), std::fclose);
  REQUIRE(magicsFile);
  ReadSeekFile magicsRs(magicsFile);
  auto magics = FileSigAnalyzer::readMagics(magicsRs);

  // Compile the program once
  auto sigProg = Lightgrep::compile(magics);

  // Create ProcessorContext with shared program
  LlamaDB db;
  LlamaDBConnection conn(db);
  DBType<HashRec>::createTable(conn.get(), "hash");
  DBType<SearchHit>::createTable(conn.get(), "search_hits");
  DBType<RuleMatch>::createTable(conn.get(), "rule_hits");
  DBType<FileSigResult>::createTable(conn.get(), "file_signatures");
  DBType<ExceptionRecord>::createTable(conn.get(), "exception_log");

  auto ruleEngine = std::make_shared<LlamaRuleEngine>();
  auto procContext = std::make_shared<ProcessorContext>(
    &db, nullptr, ruleEngine, "", "", magics, sigProg
  );

  // Verify SigMagics and SigProg were stored
  REQUIRE(procContext->SigMagics.size() > 0);
  REQUIRE(procContext->SigMagics.size() == magics.size());
  REQUIRE(procContext->SigProg);

  // Create a Processor from the context — should use shared program, not recompile
  Processor proc(procContext);
  REQUIRE(true);
}

TEST_CASE("processBatch sorts entries by DiskOffset for sequential I/O") {
  LlamaDB db;
  LlamaDBConnection conn(db);

  auto ruleEngine = std::make_shared<LlamaRuleEngine>();
  ruleEngine->createTables(conn);
  DBType<HashRec>::createTable(conn.get(), "hash");
  DBType<SearchHit>::createTable(conn.get(), "search_hits");
  DBType<FileSigResult>::createTable(conn.get(), "file_signatures");
  DBType<ExceptionRecord>::createTable(conn.get(), "exception_log");

  auto procContext = std::make_shared<ProcessorContext>(
    &db, nullptr, ruleEngine, "", "",
    MagicsType{}
  );
  Processor proc(procContext);

  // Create entries in reverse DiskOffset order: 3000, 2000, 1000
  // Each entry gets a unique Addr so we can verify order from hash table
  auto entries = std::make_shared<std::vector<std::unique_ptr<Entry>>>();

  auto e1 = std::make_unique<Entry>(100, std::make_unique<ReadSeekBuf>("aaa"));
  e1->DiskOffset = 3000;
  entries->push_back(std::move(e1));

  auto e2 = std::make_unique<Entry>(200, std::make_unique<ReadSeekBuf>("bbb"));
  e2->DiskOffset = 1000;
  entries->push_back(std::move(e2));

  auto e3 = std::make_unique<Entry>(300, std::make_unique<ReadSeekBuf>("ccc"));
  e3->DiskOffset = 2000;
  entries->push_back(std::move(e3));

  proc.processBatch(entries);

  // Query the hash table; rows are inserted in processing order
  duckdb_result result;
  duckdb_query(conn.get(), "SELECT MetaAddr FROM hash", &result);
  auto rowCount = duckdb_row_count(&result);
  REQUIRE(rowCount == 3);

  // If sorted by DiskOffset, processing order should be: 1000, 2000, 3000
  // which corresponds to Addr values: 200, 300, 100
  REQUIRE(duckdb_value_int64(&result, 0, 0) == 200);
  REQUIRE(duckdb_value_int64(&result, 0, 1) == 300);
  REQUIRE(duckdb_value_int64(&result, 0, 2) == 100);
  duckdb_destroy_result(&result);
}
