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

/*
TEST_CASE("TestSizeMatch") {
  std::shared_ptr<ProgramHandle> lg;
  Processor proc(lg);

  MockOutputHandler mock;
  FileRecord rec1;

  proc.process(rec1, mock);
  REQUIRE(1u == mock.Inodes.size());
  SCOPE_ASSERT_EQUAL({0xd8, 0x69, 0xdb, 0x7f, 0xe6, 0x2f, 0xb0, 0x7c, 0x25, 0xa0, 0x40, 0x3e, 0xca, 0xea, 0x55, 0x03, 0x17, 0x44, 0xb5, 0xfb},
                      mock.Inodes[0].Hashes.Sha1);
  SCOPE_ASSERT_EQUAL({0x00, 0x8c, 0x59, 0x26, 0xca, 0x86, 0x10, 0x23, 0xc1, 0xd2, 0xa3, 0x66, 0x53, 0xfd, 0x88, 0xe2},
                      mock.Inodes[0].Hashes.Md5);

  FileRecord rec2;
  std::fill_n(&rec2.Hashes.Md5[0], 16, 0);
  std::fill_n(&rec2.Hashes.Sha1[0], 20, 0);
  proc.process(rec2, mock);
  REQUIRE(2u == mock.Inodes.size());

  SCOPE_ASSERT_EQUAL(
    {
      0x4f, 0x41, 0x24, 0x38, 0x47, 0xda, 0x69, 0x3a,
      0x4f, 0x35, 0x6c, 0x04, 0x86, 0x11, 0x4b, 0xc6
    },
    rec2.Hashes.Md5
  );
  SCOPE_ASSERT_EQUAL(
    {
      0xf4, 0x9c, 0xf6, 0x38, 0x1e, 0x32, 0x2b, 0x14, 0x70, 0x53,
      0xb7, 0x4e, 0x45, 0x00, 0xaf, 0x85, 0x33, 0xac, 0x1e, 0x4c
    },
    rec2.Hashes.Sha1
  );

  FileRecord rec3;
  std::fill_n(&rec3.Hashes.Md5[0], 16, 0);
  std::fill_n(&rec3.Hashes.Sha1[0], 20, 0);
  proc.process(rec3, mock);
  REQUIRE(3u == mock.Inodes.size());

  SCOPE_ASSERT_EQUAL(
    {
      0x3e, 0x8a, 0x5f, 0xb7, 0xa2, 0x17, 0xc8, 0x1e,
      0x41, 0x44, 0x18, 0x37, 0xa0, 0x07, 0x83, 0x83
    },
    rec3.Hashes.Md5
  );
  SCOPE_ASSERT_EQUAL(
    {
      0xcf, 0xda, 0x77, 0x8e, 0x75, 0x58, 0x03, 0xae, 0xf7, 0x8c,
      0x9f, 0xb4, 0x16, 0x8e, 0x35, 0x90, 0x29, 0xae, 0x64, 0x99
    },
    rec3.Hashes.Sha1
  );
}
*/

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
