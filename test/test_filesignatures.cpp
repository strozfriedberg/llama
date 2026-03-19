#include <catch2/catch_test_macros.hpp>

#include "filesignatures.h"
#include "readseek_impl.h"

TEST_CASE("readMagics skips entries without patterns") {
  const char* testJson = R"([
    {"name": "With Pattern", "pattern": "TEST", "encoding": "ISO-8859-1"},
    {"name": "Without Pattern", "encoding": "ISO-8859-1"}
  ])";

  std::string jsonStr(testJson);
  ReadSeekBuf rs(jsonStr);
  auto magics = FileSigAnalyzer::readMagics(rs);

  REQUIRE(magics.size() == 1);
  REQUIRE(magics[0]->Name == "With Pattern");
}

TEST_CASE("FileSigAnalyzer detects signatures with shared program") {
  const char* testMagics = R"([
    {
      "name": "Test PDF",
      "description": "PDF test signature",
      "id": "test-pdf-001",
      "pattern": "%PDF",
      "encoding": "ISO-8859-1"
    }
  ])";

  std::string magicsStr(testMagics);
  ReadSeekBuf magicsRs(magicsStr);
  auto magics = FileSigAnalyzer::readMagics(magicsRs);

  auto sharedProg = Lightgrep::compile(magics);

  FileSigAnalyzer fromShared(sharedProg, magics);

  // Should detect PDF
  std::vector<uint8_t> pdfBytes = {0x25, 0x50, 0x44, 0x46}; // %PDF
  {
    ReadSeekBuf rs(pdfBytes);
    std::vector<MagicPtr> results;
    REQUIRE(fromShared.getSignatures(rs, results));
    REQUIRE(results.size() == 1);
    REQUIRE(results[0]->Name == "Test PDF");
  }

  // Unrecognized data should return empty
  {
    std::vector<uint8_t> garbage = {0x00, 0x01, 0x02, 0x03};
    ReadSeekBuf rs(garbage);
    std::vector<MagicPtr> results;
    REQUIRE_FALSE(fromShared.getSignatures(rs, results));
    REQUIRE(results.empty());
  }
}

TEST_CASE("Lightgrep program round-trips through serialization") {
  const char* testMagics = R"([
    {
      "name": "Test PDF",
      "description": "PDF test signature",
      "id": "test-pdf-001",
      "pattern": "%PDF",
      "encoding": "ISO-8859-1"
    }
  ])";

  std::string magicsStr(testMagics);
  ReadSeekBuf magicsRs(magicsStr);
  auto magics = FileSigAnalyzer::readMagics(magicsRs);

  auto prog = Lightgrep::compile(magics);

  // Write to temp file
  std::string tmpPath = "/tmp/test_magics_roundtrip.lgp";
  Lightgrep::writeProgram(prog, tmpPath);

  // Read back from file
  auto deserializedProg = Lightgrep::readProgram(tmpPath);

  // Construct analyzer from deserialized program and verify it detects PDF
  FileSigAnalyzer fromDeserialized(deserializedProg, magics);
  std::vector<uint8_t> pdfBytes = {0x25, 0x50, 0x44, 0x46}; // %PDF
  ReadSeekBuf rs(pdfBytes);
  std::vector<MagicPtr> results;
  REQUIRE(fromDeserialized.getSignatures(rs, results));
  REQUIRE(results.size() == 1);
  REQUIRE(results[0]->Name == "Test PDF");

  // Clean up
  std::remove(tmpPath.c_str());
}

TEST_CASE("FileSigAnalyzer detects signatures via ReadSeek") {
  const char* testMagics = R"([
    {
      "name": "Test PDF",
      "description": "PDF test signature",
      "id": "test-pdf-001",
      "pattern": "%PDF",
      "encoding": "ISO-8859-1"
    },
    {
      "name": "Test JPEG",
      "description": "JPEG test signature",
      "id": "test-jpeg-001",
      "pattern": "\\xFF\\xD8\\xFF",
      "encoding": "ISO-8859-1"
    }
  ])";

  std::string magicsStr(testMagics);
  ReadSeekBuf magicsRs(magicsStr);
  auto magics = FileSigAnalyzer::readMagics(magicsRs);
  REQUIRE(magics.size() == 2);

  auto prog = Lightgrep::compile(magics);
  FileSigAnalyzer analyzer(prog, magics);

  // Test PDF detection
  {
    std::vector<uint8_t> pdfBytes = {0x25, 0x50, 0x44, 0x46}; // %PDF
    ReadSeekBuf rs(pdfBytes);
    std::vector<MagicPtr> results;
    REQUIRE(analyzer.getSignatures(rs, results));
    REQUIRE(results.size() == 1);
    REQUIRE(results[0]->Name == "Test PDF");
  }

  // Test JPEG detection
  {
    std::vector<uint8_t> jpegBytes = {0xFF, 0xD8, 0xFF, 0xE0};
    ReadSeekBuf rs(jpegBytes);
    std::vector<MagicPtr> results;
    REQUIRE(analyzer.getSignatures(rs, results));
    REQUIRE(results.size() == 1);
    REQUIRE(results[0]->Name == "Test JPEG");
  }

  // Test unrecognized data
  {
    std::vector<uint8_t> garbage = {0x00, 0x01, 0x02, 0x03};
    ReadSeekBuf rs(garbage);
    std::vector<MagicPtr> results;
    REQUIRE_FALSE(analyzer.getSignatures(rs, results));
    REQUIRE(results.empty());
  }
}
