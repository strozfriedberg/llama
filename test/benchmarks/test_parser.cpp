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

TEST_CASE("LlamaParserBenchmark") {
  RuleReader r;
  int res = 0;
  BENCHMARK("parser") {
    res = r.read(rules);
    r.clear();
  };
  CHECK(res == 4);
  CHECK(r.getLastError() == "");
}