#include <catch2/catch_test_macros.hpp>

#include <sstream>

#include "ducksig.h"
#include "filesignatures.h"
#include "llama.h"
#include "llamaduck.h"
#include "readseek_impl.h"
#include "ruleengine.h"

TEST_CASE("testReadDirPopulatesRulesCorrectly") {
  std::string testDir = "test/rules";
  std::shared_ptr<LlamaRuleEngine> engine = std::make_shared<LlamaRuleEngine>(LlamaRuleEngine());
  readRulesFromDir(engine, testDir);
  REQUIRE(engine->numRulesRead() == 2);
}

TEST_CASE("Signature reference table is populated from magics") {
  LlamaDB db;
  LlamaDBConnection conn(db);

  DBType<SigRec>::createTable(conn.get(), "signatures");

  std::shared_ptr<FILE> magicsFile(std::fopen("./magics.json", "rb"), std::fclose);
  REQUIRE(magicsFile);
  ReadSeekFile rs(magicsFile);
  auto magics = FileSigAnalyzer::readMagics(rs);

  // Populate the signatures table
  LlamaDBAppender appender(conn.get(), "signatures");
  SigBatch batch;
  for (const auto& m : magics) {
    batch.add(SigRec{m->Id, m->Name, m->Description});
  }
  batch.copyToDB(appender.get());
  appender.flush();

  duckdb_result result;
  duckdb_query(conn.get(), "SELECT count(*) FROM signatures", &result);
  auto count = duckdb_value_int64(&result, 0, 0);
  duckdb_destroy_result(&result);

  REQUIRE(count == static_cast<int64_t>(magics.size()));
  REQUIRE(count > 0);
}
