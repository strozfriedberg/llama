
#include <fstream>
#include <jsoncons/json.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>

#include "filesignatures.h"
#include "readseek_impl.h"

namespace {
using namespace FileSignatures;
namespace fs = std::filesystem;

template <typename ItemType>
struct DataGenerator : public Catch::Generators::IGenerator<ItemType> {
  jsoncons::json data;
  jsoncons::json::const_array_iterator current_ptr;
  ItemType current;

  DataGenerator(std::string_view path) : data(std::vector<int>()) {
    std::ifstream is(path.data());

    if (is.fail())
      throw std::runtime_error(std::string("Error: bad path ") +
                               std::string(path));

    data = jsoncons::json(jsoncons::json::parse(is));
    if ((current_ptr = data.array_range().cbegin()) == data.array_range().end())
      throw std::runtime_error(std::string("Error: ") + std::string(path) +
                               " has no data");
  }

  ItemType const &get() const override { return current; }

  bool next() override {
      if (current_ptr == data.array_range().end())
          return false;
      current = ItemType(*current_ptr++);
      return true;
  }
};

struct LengthItem {
  std::string Pattern;
  size_t Len;

  LengthItem() = default;
  LengthItem(jsoncons::json const &json)
      : Pattern(json.at(0).as<std::string>()), Len(json.at(1).as<int>()) {}
};

using LengthDataGenerator = DataGenerator<LengthItem>;
} // namespace

TEST_CASE("Compare with verified data", "[getPatternLength]") {
  auto generator = LengthDataGenerator("test/data/pattern_lengths.json");
  while (generator.next()) {
    auto data = generator.get();
    REQUIRE(::getPatternLength(data.Pattern, true) == data.Len);
  }
}

TEST_CASE("readMagics skips entries without patterns") {
  const char* testJson = R"([
    {"name": "With Pattern", "pattern": "TEST", "encoding": "ISO-8859-1"},
    {"name": "Without Pattern", "encoding": "ISO-8859-1"}
  ])";

  std::string jsonStr(testJson);
  ReadSeekBuf rs(jsonStr);
  auto result = FileSignatures::FileSigAnalyzer::readMagics(rs);
  REQUIRE(result.has_value());
  auto& magics = result.value();

  REQUIRE(magics.size() == 1);
  REQUIRE(magics[0]->Name == "With Pattern");
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
  auto magicsResult = FileSignatures::FileSigAnalyzer::readMagics(magicsRs);
  REQUIRE(magicsResult.has_value());
  auto& magics = magicsResult.value();
  REQUIRE(magics.size() == 2);

  FileSignatures::FileSigAnalyzer analyzer(magics);

  // Test PDF detection
  {
    std::vector<uint8_t> pdfBytes = {0x25, 0x50, 0x44, 0x46}; // %PDF
    ReadSeekBuf rs(pdfBytes);
    std::vector<FileSignatures::MagicPtr> results;
    auto ok = analyzer.getSignatures(rs, results);
    REQUIRE(ok.has_value());
    REQUIRE(results.size() == 1);
    REQUIRE(results[0]->Name == "Test PDF");
  }

  // Test JPEG detection
  {
    std::vector<uint8_t> jpegBytes = {0xFF, 0xD8, 0xFF, 0xE0};
    ReadSeekBuf rs(jpegBytes);
    std::vector<FileSignatures::MagicPtr> results;
    auto ok = analyzer.getSignatures(rs, results);
    REQUIRE(ok.has_value());
    REQUIRE(results.size() == 1);
    REQUIRE(results[0]->Name == "Test JPEG");
  }

  // Test unrecognized data
  {
    std::vector<uint8_t> garbage = {0x00, 0x01, 0x02, 0x03};
    ReadSeekBuf rs(garbage);
    std::vector<FileSignatures::MagicPtr> results;
    auto ok = analyzer.getSignatures(rs, results);
    REQUIRE(ok.has_value());
    REQUIRE(results.empty());
  }
}