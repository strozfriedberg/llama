#include <catch2/benchmark/catch_benchmark_all.hpp>
#include <catch2/catch_test_macros.hpp>

#include "rulereader.h"
#include "rule_generator.h"
#include "querybuilder.h"

std::string rules(R"(
rule Malware {
  file_metadata:
    created > "2023-04-05" and filename == "bad.exe"
  grep:
    patterns:
      p1 = "8.8.8.8"
    condition:
      any(p1)
}

rule sedexp {
  meta:
    description = "Rule to find sedexp malware"
    source = "https://www.aon.com/en/insights/cyber-labs/unveiling-sedexp"
  hash:
    sha256 == "43f72f4cdab8ed40b2f913be4a55b17e7fd8a7946a636adb4452f685c1ffea02"
    sha256 == "94ef35124a5ce923818d01b2d47b872abd5840c4f4f2178f50f918855e0e5ca2"
    sha256 == "b981948d51e344972d920722385f2370caf1e4fac0781d508bc1f088f477b648"
}

rule WebShell {
  hash:
    sha256 == "43f72f4cdab8ed40b2f913be4a55b17e7fd8a7946a636adb4452f685c1ffea02"
  signature:
    name == "Active Server Page"
  grep:
    patterns:
      p1 = "<?php" fixed
    condition:
      all(p1)
}

rule BetterRule {
  meta:
    description = "Another rule to find sedexp malware"
    source = "https://www.aon.com/en/insights/cyber-labs/unveiling-sedexp"
    author = "me"

  hash:
    sha256 == "43f72f4cdab8ed40b2f913be4a55b17e7fd8a1236a636adb4452f685c1ffea02", md5 == "098f6bcd4621d373cade4e832627b4f6"

  file_metadata:
    filesize > 1000

  signature:
    name == "Executable"

  grep:
    patterns:
      p1 = "crypto.h" fixed
      p2 = { 12 34 56 78 90 AB CD EF }
      p3 = "whoami" encodings=utf8,utf16,windows-1252
    condition:
      all(p1) or any(p1, p2, p3) or (count(p3) > 5 and offset(p2, 3) > 60)
}
)");

TEST_CASE("LlamaLexerBenchmark") {
  LlamaLexer lexer;
  int res = 0;
  BENCHMARK("lexer") {
    lexer.setInput(rules);
    lexer.scanTokens();
    res = lexer.tokens().size();
    lexer.clear();
  };
  CHECK(res == 175);
  CHECK(lexer.errors().empty());
}

TEST_CASE("LlamaParser") {
  LlamaLexer lexer;
  lexer.setInput(rules);
  lexer.scanTokens();
  CHECK(lexer.errors().empty());

  LlamaParser parser(rules, lexer.tokens());
  std::vector<Rule> parsed;
  BENCHMARK("parser") {
    parsed = parser.parseRules(lexer.ruleIndices());
    parser.clear();
  };
  CHECK(parsed.size() == 5);
}

TEST_CASE("LlamaParserBenchmark") {
  RuleReader r;
  bool res = false;
  BENCHMARK("RuleReader") {
    res = r.read(rules, "test");
    r.clear();
  };
  CHECK(res);
}

TEST_CASE("LargeCorpusBenchmark") {
  // Generate 100 rules for realistic workload
  std::string largeCorpus = generateRules(100);

  RuleReader r;
  bool res = false;
  size_t ruleCount = 0;

  BENCHMARK("RuleReader-100rules") {
    res = r.read(largeCorpus, "benchmark");
    ruleCount = r.getRules().size();
    r.clear();
  };

  CHECK(res);
  CHECK(ruleCount == 100);
}

TEST_CASE("ScalingBenchmark") {
  // Test scaling behavior with different rule counts
  std::string corpus10 = generateRules(10);
  std::string corpus50 = generateRules(50);
  std::string corpus100 = generateRules(100);
  std::string corpus500 = generateRules(500);

  RuleReader r;

  BENCHMARK("RuleReader-10rules") {
    r.read(corpus10, "benchmark");
    r.clear();
  };

  BENCHMARK("RuleReader-50rules") {
    r.read(corpus50, "benchmark");
    r.clear();
  };

  BENCHMARK("RuleReader-100rules-scaling") {
    r.read(corpus100, "benchmark");
    r.clear();
  };

  BENCHMARK("RuleReader-500rules") {
    r.read(corpus500, "benchmark");
    r.clear();
  };
}

TEST_CASE("SqlGenerationBenchmark") {
  // Use rules with file_metadata to exercise SQL generation
  std::string corpus = generateRules(100);

  RuleReader r;
  REQUIRE(r.read(corpus, "benchmark"));

  QueryBuilder qb(r.getParser());
  const auto& rules = r.getRules();
  std::string query;

  BENCHMARK("buildSqlQuery-100rules") {
    std::string hits_query("INSERT INTO rule_hits ");
    const std::string sqlUnion(" UNION ");

    for (const Rule& rule : rules) {
      hits_query += "(";
      hits_query += qb.buildSqlQuery(rule);
      hits_query += ")";
      hits_query += sqlUnion;
    }
    hits_query.erase(hits_query.size() - sqlUnion.size());
    hits_query += ";";
    query = std::move(hits_query);
  };

  CHECK(!query.empty());
}

