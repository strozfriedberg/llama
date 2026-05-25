#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <string>

#include "ruleengine.h"
#include "llamaduck.h"
#include "rulereader.h"
#include "inode.h"
#include "direntbatch.h"
#include "duckinode.h"

namespace {
  std::array<uint8_t, 32> bytesFromHex32(const std::string& hex) {
    std::array<uint8_t, 32> out{};
    for (size_t i = 0; i < 32; ++i) {
      auto hexVal = [](char c) -> uint8_t {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return 0;
      };
      out[i] = (hexVal(hex[2 * i]) << 4) | hexVal(hex[2 * i + 1]);
    }
    return out;
  }
}

TEST_CASE("TestCreateTables") {
  LlamaRuleEngine engine;
  LlamaDB db;
  LlamaDBConnection conn(db);
  engine.createTables(conn);
  duckdb_result result;
  auto state = duckdb_query(conn.get(), "select * from rules;", &result);
  REQUIRE(state == DuckDBSuccess);
  REQUIRE(duckdb_row_count(&result) == 0);
  state = duckdb_query(conn.get(), "select * from rule_hits;", &result);
  REQUIRE(state == DuckDBSuccess);
  REQUIRE(duckdb_row_count(&result) == 0);
  state = duckdb_query(conn.get(), "select * from search_hits;", &result);
  REQUIRE(state == DuckDBSuccess);
  REQUIRE(duckdb_row_count(&result) == 0);
  REQUIRE_THROWS(engine.createTables(conn));
}

TEST_CASE("TestWriteRuleToDb") {
  std::string input = "rule MyRule { file_metadata: created > \"2021-01-01\" } rule MyRule2 { file_metadata: filesize > 100 }";
  LlamaRuleEngine engine;
  engine.read(input, "test");
  LlamaDB db;
  LlamaDBConnection conn(db);
  REQUIRE(DBType<Dirent>::createTable(conn.get(), "dirent"));
  REQUIRE(DBType<Inode>::createTable(conn.get(), "inode"));
  engine.createTables(conn);
  engine.writeRulesToDb(conn);
  duckdb_result result;
  auto state = duckdb_query(conn.get(), "select * from rules;", &result);
  CHECK(state == DuckDBSuccess);
  CHECK(duckdb_result_error(&result) == nullptr);
  REQUIRE(duckdb_row_count(&result) == 2);
  state = duckdb_query(conn.get(), "select * from rule_hits;", &result);
  REQUIRE(state == DuckDBSuccess);
  REQUIRE(duckdb_row_count(&result) == 0);
}

TEST_CASE("TestZeroRulesToDb") {
  RuleReader reader;
  LlamaRuleEngine engine;
  LlamaDB    db;
  LlamaDBConnection conn(db);

  REQUIRE_NOTHROW(engine.createTables(conn));
  REQUIRE_NOTHROW(engine.writeRulesToDb(conn));
}

TEST_CASE("getFsm") {
  std::string input = R"(
  rule myRule {
    grep:
      patterns:
        a = "test" encodings=UTF-8,UTF-16LE
      condition:
        any()
    }
  rule MyOtherRule {
    grep:
      patterns:
        a = "foobar" fixed
      condition:
        any()
  })";
  LlamaRuleEngine engine;
  engine.read(input, "test");
  LgFsmHolder fsmHolder = engine.buildFsm();
  REQUIRE(lg_fsm_pattern_count(fsmHolder.getFsm()) == 3);
  auto patToRuleId = engine.patternToRuleId();
  REQUIRE(patToRuleId.size() == 3);
  REQUIRE(patToRuleId[0] == patToRuleId[1]);
  REQUIRE(patToRuleId[1] != patToRuleId[2]);
}

TEST_CASE("createTables creates signature tables") {
  LlamaDB db;
  LlamaDBConnection conn(db);
  LlamaRuleEngine engine;
  engine.createTables(conn);

  // Verify signature tables exist by inserting into them
  duckdb_result result;
  auto state = duckdb_query(conn.get(),
    "INSERT INTO signatures VALUES ('id-1', 'PDF', 'Portable Document Format')", &result);
  REQUIRE(state != DuckDBError);
  duckdb_destroy_result(&result);

  state = duckdb_query(conn.get(),
    "INSERT INTO file_signatures VALUES ('hash-1', 'id-1')", &result);
  REQUIRE(state != DuckDBError);
  duckdb_destroy_result(&result);
}

TEST_CASE("rule_hits insert with real dirent and inode match") {
  // Known 32-byte sentinel for the inode identity (MetaId / inode.Id).
  const std::string knownHex = "1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef";
  const auto knownId = bytesFromHex32(knownHex);

  // A rule whose WHERE clause matches rows with Filesize > 0.
  std::string ruleInput = "rule MatchBigFiles { file_metadata: filesize > 0 }";

  LlamaRuleEngine engine;
  engine.read(ruleInput, "test");

  LlamaDB db;
  LlamaDBConnection conn(db);
  REQUIRE(DBType<Dirent>::createTable(conn.get(), "dirent"));
  REQUIRE(DBType<Inode>::createTable(conn.get(), "inode"));
  engine.createTables(conn);

  // Populate dirent: one row with MetaId = knownId.
  {
    Dirent d{};
    d.Id       = knownId;   // dirent's own hash (arbitrary, not JOIN key)
    d.MetaId   = knownId;   // FK into inode.Id
    d.ParentId = knownId;   // arbitrary
    d.Path     = "/docs/";
    d.Name     = "report.pdf";
    d.ShortName = "report.pdf";
    d.Type     = "r";
    d.Flags    = "";
    d.MetaAddr = 42;
    d.ParentAddr = 5;
    d.MetaSeq  = 1;
    d.ParentSeq = 1;

    DirentBatch batch;
    batch.add(d);
    LlamaDBAppender appender(conn.get(), "dirent");
    batch.copyToDB(appender.get());
    appender.flush();
  }

  // Populate inode: one row with Id = knownId and Filesize > 0.
  {
    Inode in{};
    in.Id               = knownId;
    in.Type             = "r";
    in.Flags            = "";
    in.Addr             = 42;
    in.EvidenceFileName = "disk.E01";
    in.ByteOffset       = 0;
    in.Filesize         = 1024;
    in.Uid              = 0;
    in.Gid              = 0;
    in.LinkTarget       = "";
    in.NumLinks         = 1;
    in.SeqNum           = 1;
    in.Created          = "";
    in.Accessed         = "";
    in.Modified         = "";
    in.Metadata         = "";

    InodeBatch batch;
    batch.add(in);
    LlamaDBAppender appender(conn.get(), "inode");
    batch.copyToDB(appender.get());
    appender.flush();
  }

  // Run the rule engine: writes rules to DB and executes the INSERT INTO rule_hits SELECT.
  engine.writeRulesToDb(conn);

  duckdb_result result;

  // Exactly one rule_hits row should exist.
  auto state = duckdb_query(conn.get(), "SELECT COUNT(*) FROM rule_hits;", &result);
  REQUIRE(state == DuckDBSuccess);
  REQUIRE(duckdb_row_count(&result) == 1);
  REQUIRE(duckdb_value_int64(&result, 0, 0) == 1);
  duckdb_destroy_result(&result);

  // The inode_id BLOB must equal our sentinel and be exactly 32 bytes.
  std::string blobCheck =
    "SELECT octet_length(inode_id), path, name FROM rule_hits "
    "WHERE inode_id = unhex('" + knownHex + "');";
  state = duckdb_query(conn.get(), blobCheck.c_str(), &result);
  REQUIRE(state == DuckDBSuccess);
  REQUIRE(duckdb_row_count(&result) == 1);
  REQUIRE(duckdb_value_int64(&result, 0, 0) == 32);  // octet_length
  REQUIRE(std::string(duckdb_value_varchar(&result, 1, 0)) == "/docs/");  // path
  REQUIRE(std::string(duckdb_value_varchar(&result, 2, 0)) == "report.pdf");  // name
  duckdb_destroy_result(&result);
}