#include <catch2/benchmark/catch_benchmark_all.hpp>
#include <catch2/catch_test_macros.hpp>

#include "rulereader.h"

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
)");

TEST_CASE("LlamaParserBenchmark") {
  RuleReader r;
  int res = 0;
  BENCHMARK("parser") {
    res = r.read(rules);
    r.clear();
  };
  CHECK(res == 3);
  CHECK(r.getLastError() == "");
}