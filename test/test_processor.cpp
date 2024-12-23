#include <catch2/catch_test_macros.hpp>

#include "processor.h"

#include "lightgrep/api.h"
#include "filerecord.h"
#include "mockoutputhandler.h"
#include "readseek_impl.h"
#include "patternparser.h"

#include <hasher/api.h>

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
  : PatternToRuleId(numExpectedHits, "rule_id"), RsBuf(haystack), Db(), DbConn(Db), Proc(createProcessor(needle)) {
    Proc.currentHash("file_hash");
  }

  void search() { Proc.search(RsBuf); }
  uint64_t putSearchHitsInDb() {
    DBType<SearchHit>::createTable(DbConn.get(), "search_hits");
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
    DBType<HashRec>::createTable(DbConn.get(), "hash");
    return Processor{&Db, pHandle, PatternToRuleId};
  }
  std::vector<std::string> PatternToRuleId;
  ReadSeekBuf RsBuf;
  LlamaDB Db;
  LlamaDBConnection DbConn;
  Processor Proc;
};

TEST_CASE("testBasicOneHitSearch") {
  ProcessorSearchTester pst{"foobar", "this is so foobar", 1};
  pst.search();
  REQUIRE(1 == pst.putSearchHitsInDb());
  std::vector<SearchHit> expectedSearchHits = {
    SearchHit{"foobar", 11, 17, "rule_id", "file_hash", 6}
  };
  pst.createTempTableAndPopulate(expectedSearchHits);
  REQUIRE(0 == pst.numDiffsBetweenTables());
}

TEST_CASE("testSearchThatSpansMultipleBuffers") {
  std::string needle = "a+";

  // arbitrary length over the buffer size
  uint64_t hitLength = (1 << 20) + 4;

  std::vector<char> v(hitLength, 'a');
  std::string haystack(v.begin(), v.end());
  CHECK(hitLength == haystack.size());
  uint64_t numExpectedHits = 1;

  ProcessorSearchTester pst{needle, haystack, numExpectedHits};
  pst.search();
  REQUIRE(numExpectedHits == pst.putSearchHitsInDb());
  std::vector<SearchHit> expectedSearchHits = {
    SearchHit{needle, 0, hitLength, "rule_id", "file_hash", hitLength}
  };
  pst.createTempTableAndPopulate(expectedSearchHits);
  REQUIRE(0 == pst.numDiffsBetweenTables());
}
