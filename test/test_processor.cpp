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

bool numDiffsBetweenTables(duckdb_connection dbConn, std::string table1, std::string table2) {
  duckdb_result result;
  std::string diffUnionQuery = "(Select * from " + table1 + " Except Select * from " + table2 + " ) UNION ALL (Select * from " + table1 + " Except Select * from " + table2 + " )";
  duckdb_query(dbConn, (diffUnionQuery + ";").c_str(), &result);
  auto diffs = duckdb_row_count(&result);
  duckdb_destroy_result(&result);
  return diffs;
}

TEST_CASE("testSearch") {
  std::string sPat = "foobar";
  std::string haystack = "this is soooo foobar";
  ReadSeekBuf rsBuf(haystack);

  // lg setup
  LG_HPATTERN pat(lg_create_pattern());
  LG_KeyOptions opts{0,0,0};
  LG_Error* err(nullptr);
  lg_parse_pattern(pat, sPat.c_str(), &opts, &err);
  LG_HFSM fsm(lg_create_fsm(0, 0));
  lg_add_pattern(fsm, pat, "ASCII", 0, &err);
  LG_ProgramOptions pOpts{10};
  LG_HPROGRAM prog(lg_create_program(fsm, &pOpts));
  std::shared_ptr<ProgramHandle> pHandle(prog, lg_destroy_program);

  // duckdb setup
  LlamaDB db;
  LlamaDBConnection dbConn(db);
  DBType<HashRec>::createTable(dbConn.get(), "hash");
  std::vector<std::string> patToRuleId{"rule_id"};
  Processor proc(&db, pHandle, patToRuleId);
  proc.currentHash("file_hash");

  // search
  proc.search(rsBuf);
  THROW_IF(!DBType<SearchHit>::createTable(dbConn.get(), "search_hits"), "Error creating search hit table");
  LlamaDBAppender appender(dbConn.get(), "search_hits");
  CHECK(1 == proc.searchHits()->copyToDB(appender.get()));
  appender.flush();

  // temp table for insert validation
  DBBatch<SearchHit> tempSearchHits;
  DBType<SearchHit>::createTable(dbConn.get(), "temp_search_hits");
  tempSearchHits.add(SearchHit{"foobar", 14, 20, "rule_id", "file_hash", 6});
  LlamaDBAppender tempAppender(dbConn.get(), "temp_search_hits");
  CHECK(1 == tempSearchHits.copyToDB(tempAppender.get()));
  tempAppender.flush();

  // validate that diff with temp table is 0
  CHECK(0 == numDiffsBetweenTables(dbConn.get(), "search_hits", "temp_search_hits"));

  // cleanup
  lg_destroy_pattern(pat);
  lg_destroy_fsm(fsm);
}
