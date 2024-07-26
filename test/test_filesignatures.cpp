
#include <fstream>
#include <jsoncons/json.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>

#include "filesignatures.h"

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
    current = ItemType(*current_ptr);
    return ++current_ptr != data.array_range().end();
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

struct TestSignItem {
  Binary binary;
  std::string expected_ext;
  std::optional<std::string> signature_id;

  TestSignItem() = default;
  TestSignItem(jsoncons::json const &json) {
    binary = str2bin(json["data"].as_string());
    expected_ext = json["expected_ext"].as_string();
    auto sign = json["signature"];
    if (!sign.is_null()) {
      signature_id = sign["id"].as_string();
    }
  }
};
using TestSignDataGenerator = DataGenerator<TestSignItem>;

struct tempfile : std::fstream {
  fs::path Filename;

  tempfile(Binary const &data, std::string const &ext) {
    Filename = fs::temp_directory_path() / fs::path("test_filesign." + ext);
    open(Filename, this->binary | this->trunc | this->out);
    write(reinterpret_cast<char const *>(data.data()), data.size());
    close();
  }
  ~tempfile() { std::filesystem::remove(Filename); }
};
} // namespace

TEST_CASE("Compare with verified signatures", "[testSignatures]") {
  auto generator = TestSignDataGenerator("test/data/test_signatures.json");
  FileSigAnalyzer file_sig_analyzer;

  while (generator.next()) {
    auto data = generator.get();
    tempfile file(data.binary, data.expected_ext);
    std::filesystem::directory_entry de(file.Filename);
    MagicPtr result;
    if (file_sig_analyzer.getSignature(de, result).value()) {
      REQUIRE(data.signature_id.has_value() == true);
      REQUIRE(result->Id == data.signature_id.value());
    } else {
      REQUIRE(data.signature_id.has_value() == false);
    }
  }
}

TEST_CASE("Compare with verified data", "[getPatternLength]") {
  auto generator = LengthDataGenerator("test/data/pattern_lengths.json");
  while (generator.next()) {
    auto data = generator.get();
    REQUIRE(::getPatternLength(data.Pattern, true) == data.Len);
  }
}

TEST_CASE("Parsing offsets", "[parseOffset]") {
  std::string magics_file("./magics.json");
  FileSigAnalyzer fsa;
  auto result = fsa.readMagics(magics_file);

  if (result.has_error()) {
    throw std::runtime_error("Couldn't open file: " + magics_file +
                             std::string(", ") + result.error());
  }

  auto offsets_json = "test/data/offsets.json";
  std::ifstream is(offsets_json);

  if (is.fail())
    throw std::runtime_error(std::string("Error: offsets_json bad path ") +
                             std::string(offsets_json));

  auto const &magics = result.value();
  auto const json = jsoncons::json(jsoncons::json::parse(is));

  for (auto magic : magics) {
    if (magic->Checks.size() > 0) {
      auto const &item_ptr = json.find(magic->Id);
      if (item_ptr != json.object_range().end()) {
        auto item_range = (*item_ptr).value().array_range();
        for (auto item = item_range.cbegin(); item != item_range.cend();
             ++item) {
          auto check_item = item->array_range().cbegin();
          std::string str_offset = check_item[0].as_cstring();
          long int_offset = check_item[1].as_integer<long>();
          int whence = check_item[2].as_integer<int>();
          auto i = item - item_range.cbegin();
          REQUIRE(int_offset == magic->Checks[i].Offset.count);
          REQUIRE(whence == (magic->Checks[i].Offset.from_start ? 0 : 2));
        }
      }
    }
  }
}