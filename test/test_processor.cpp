#include <catch2/catch_test_macros.hpp>

#include "processor.h"

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
#include <filesystem>

#include "boost_asio.h"

#include "boost/interprocess/file_mapping.hpp"
#include "boost/interprocess/mapped_region.hpp"

namespace bip = boost::interprocess;


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
    Proc.setBlake3("file_hash");
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

    auto procContext = std::make_shared<ProcessorContext>(&Db, pHandle, PatternToRuleId, "", "");
    return Processor(procContext);
  }
  std::vector<std::string> PatternToRuleId;
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

TEST_CASE("testCheckHset") {
  SFHASH_HashValues Hashes;
  std::string blake3 = "4878ca0425c739fa427f7eda20fe845f6b2e46ba5fe2a14df5b1e32f50603215";
  sfhash_unhex(Hashes.Blake3, blake3.c_str(), blake3.size());

  const char* fileName = "actual.hset";
  bip::file_mapping fm(fileName, bip::read_only);
  bip::mapped_region mr(fm, bip::read_only);

  uint8_t* beg = reinterpret_cast<uint8_t*>(mr.get_address());
  uint8_t* end = beg + mr.get_size();

  SFHASH_Error* err = nullptr;
  const SFHASH_Hashset* hset = sfhash_load_hashset(beg, end, &err);
  REQUIRE(!err);

  size_t tidx = sfhash_hashset_index_for_type(hset, SFHASH_BLAKE3);

  REQUIRE(sfhash_hashset_count(hset, tidx) == 3);
  REQUIRE(std::string(sfhash_hashset_name(hset)) == "Test");
  REQUIRE(std::string(sfhash_hashset_description(hset)) == "test for llama");
  REQUIRE(sfhash_hashset_lookup(hset, tidx, Hashes.Blake3));
}
