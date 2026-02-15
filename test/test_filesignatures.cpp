
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
  auto result = FileSignatures::FileSigAnalyzer::readMagics("./magics.json");
  REQUIRE(result.has_value());
  auto& magics = result.value();
  for (const auto& m : magics) {
    REQUIRE_FALSE(m->Pattern.empty());
  }
}