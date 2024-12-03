#include <catch2/catch_test_macros.hpp>

#include "cli.h"

#include <regex>
#include <sstream>
#include <thread>

TEST_CASE("testCLIVersion") {
  const char* args[] = {"llama", "--version"};
  Cli cli;
  auto opts = cli.parse(2, args);
  REQUIRE("version" == opts->Command);
}

TEST_CASE("testCLIHelp") {
  const char* args[] = {"llama", "--help"};
  Cli cli;
  auto opts = cli.parse(2, args);
  REQUIRE("help" == opts->Command);
}

TEST_CASE("testCLICommandPrecedence") {
  // help has precedence over version
  const char* args1[] = {"llama", "--version", "--help"};
  const char* args2[] = {"llama", "--help", "--version"};
  Cli cli;
  auto opts = cli.parse(3, args1);
  REQUIRE("help" == opts->Command);
  opts = cli.parse(3, args2);
  REQUIRE("help" == opts->Command);
}

TEST_CASE("testCLIDefaultCommand") {
  const char* args[] = {"llama", "output", "nosnits_workstation.E01"};
  Cli cli;
  auto opts = cli.parse(3, args);
  REQUIRE("search" == opts->Command);
  REQUIRE("output" == opts->Output);
  REQUIRE("nosnits_workstation.E01" == opts->Input);
}

TEST_CASE("testNoArgs") {
  const char* args[] = {"llama"};
  Cli cli;
  CHECK_THROWS_AS(cli.parse(1, args), std::invalid_argument);
}

TEST_CASE("testNoInput") {
  const char* args[] = {"llama", "output"};
  Cli cli;
  CHECK_THROWS_AS(cli.parse(2, args), std::invalid_argument);
}

TEST_CASE("testCLIKeywordsFiles") {
  const char* args[] = {"llama", "-k", "mypatterns.txt", "--keywords-file",
                        "morepatterns.txt", "output", "nosnits_workstation.E01"};
  std::vector<std::string> expected{"mypatterns.txt", "morepatterns.txt"};
  Cli cli;
  auto opts = cli.parse(7, args);
  REQUIRE(expected == opts->KeyFiles);
}

TEST_CASE("testCLIRuleFiles") {
  const char* args[] = {"llama", "--rule-file",
                        "rules.txt", "output", "nosnits_workstation.E01"};
  std::string expected("rules.txt");
  Cli cli;
  auto opts = cli.parse(5, args);
  REQUIRE(expected == opts->RuleFile);
}

TEST_CASE("testCLIRuleDir") {
  const char* args[] = {"llama", "--rule-dir",
                        "rules/", "output", "nosnits_workstation.E01"};
  std::string expected("rules/");
  Cli cli;
  auto opts = cli.parse(5, args);
  REQUIRE(expected == opts->RuleDir);
}

TEST_CASE("testCLIReal") {
  const char* args[] = {"llama", "-f", "patterns.txt", "output.tar",
                        "nosnits_workstation.E01"};
  Cli cli;
  auto opts = cli.parse(5, args);
  REQUIRE("nosnits_workstation.E01" == opts->Input);
}

TEST_CASE("testCLInumThreads") {
  const char* args1[] = {"llama", "-j", "17", "output", "nosnits_workstation.E01"};
  Cli cli;
  auto opts = cli.parse(5, args1);
  REQUIRE(17u == opts->NumThreads);

  const char* args2[] = {"llama", "output", "nosnits_workstation.E01"};
  opts = cli.parse(3, args2); // test default
  REQUIRE(std::thread::hardware_concurrency() == opts->NumThreads);
}

std::ostream& operator<<(std::ostream& out, Codec c) {
  return out << static_cast<int>(c);
}

TEST_CASE("testCLICodec") {
  Cli cli;

  const char* gzipArgs[] = {"llama", "--codec=gzip", "output.tar", "nosnits_workstation.E01"};
  auto opts = cli.parse(4, gzipArgs);
  CHECK(Codec::GZIP == opts->OutputCodec);

  const char* lz4Args[] = {"llama", "--codec=lz4", "output.tar", "nosnits_workstation.E01"};
  opts = cli.parse(4, lz4Args);
  CHECK(Codec::LZ4 == opts->OutputCodec);

  const char* noneArgs[] = {"llama", "--codec=none", "output.tar", "nosnits_workstation.E01"};
  opts = cli.parse(4, noneArgs);
  CHECK(Codec::NONE == opts->OutputCodec);

  const char* lzmaArgs[] = {"llama", "--codec=lzma", "output.tar", "nosnits_workstation.E01"};
  opts = cli.parse(4, lzmaArgs);
  CHECK(Codec::LZMA == opts->OutputCodec);

  const char* bz2Args[] = {"llama", "--codec=bzip2", "output.tar", "nosnits_workstation.E01"};
  opts = cli.parse(4, bz2Args);
  CHECK(Codec::BZIP2 == opts->OutputCodec);

  const char* lzoArgs[] = {"llama", "--codec=lzo", "output.tar", "nosnits_workstation.E01"};
  opts = cli.parse(4, lzoArgs);
  CHECK(Codec::LZOP == opts->OutputCodec);

  const char* xzArgs[] = {"llama", "--codec=xz", "output.tar", "nosnits_workstation.E01"};
  opts = cli.parse(4, xzArgs);
  CHECK(Codec::XZ == opts->OutputCodec);

  const char* badArgs[] = {"llama", "--codec=bad", "output.tar", "nosnits_workstation.E01"};
  CHECK_THROWS_AS(opts = cli.parse(4, badArgs), std::invalid_argument);

  const char* defaultArgs[] = {"llama", "output.tar", "nosnits_workstation.E01"};
  opts = cli.parse(3, defaultArgs);
  CHECK(Codec::LZ4 == opts->OutputCodec);
}

TEST_CASE("testPrintVersion") {
  Cli cli;
  std::stringstream output;
  cli.printVersion(output);
  auto outStr = output.str();
  std::regex re("pre-alpha\\s\\w{3}\\s\\s?\\d{1,2}\\s20\\d{2}");
  REQUIRE(std::regex_search(outStr, re));
}

TEST_CASE("testPrintHelp") {
  // The mantra is to test everything that could possibly fail. It's unlikely
  // that boost::program_options will fail, so we just need to do a smoke test.
  Cli cli;
  std::stringstream output;
  cli.printHelp(output);
  auto outStr = output.str();
  REQUIRE(outStr.find("Usage: llama") != std::string::npos);
}
