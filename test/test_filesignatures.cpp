
#include <jsoncons/json.hpp>
#include <fstream>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>

#include "filesignatures.h"

namespace {
    struct Item {
        std::string pattern;
        size_t len;
        Item() = default;
        Item(std::string const& pattern_, size_t len_) :pattern(pattern_), len(len_) { }
    };

    struct VerifiedDataGenerator : public Catch::Generators::IGenerator<Item> {
        jsoncons::json data;
        jsoncons::json::const_array_iterator current_ptr;
        Item current;

        VerifiedDataGenerator(std::string_view path) : data(std::vector<int>()) {
            std::ifstream is(path.data());

            if (is.fail())
                throw std::runtime_error(std::string("Error: bad path ") + std::string(path));

            data = jsoncons::json(jsoncons::json::parse(is));
            current_ptr = data.array_range().cbegin();
        }

        Item const& get() const override {
            return current;
        }

        bool next() override {
            current_ptr++;
            if (current_ptr == data.array_range().end())
                return false;

            auto pair = *current_ptr;
            current = Item(pair.at(0).as<std::string>(), pair.at(1).as<int>());
            return true;
        }
    };
}

TEST_CASE("Compare with verified data", "[get_pattern_length]") {
    auto generator = VerifiedDataGenerator("test/data/pattern_lengths.json");
    while (generator.next()) {
        auto data = generator.get();
        REQUIRE(::get_pattern_length(data.pattern, true) == data.len);
    }
}

TEST_CASE("Parsing offsets", "[parseOffset]") {
    std::string magics_file("./magics.json");
    FileSigAnalyzer fsa;
    auto result = fsa.readMagics(magics_file);

    if (result.has_error()) {
        throw std::runtime_error("Couldn't open file: " + magics_file + std::string(", ") + result.error());
    }

    auto offsets_json = "test/data/offsets.json";
    std::ifstream is(offsets_json);

    if (is.fail())
        throw std::runtime_error(std::string("Error: offsets_json bad path ") + std::string(offsets_json));

    auto const & magics = result.value();
    auto const json = jsoncons::json(jsoncons::json::parse(is));

    for (auto magic : magics) {
        if (magic->checks.size() > 0) {
            auto const & item_ptr = json.find(magic->id);
            if (item_ptr != json.object_range().end()) {
                auto item_range = (*item_ptr).value().array_range();
                for (auto item = item_range.cbegin(); item != item_range.cend(); ++item) {
                    auto check_item = item->array_range().cbegin();
                    std::string str_offset = check_item[0].as_cstring();
                    long int_offset = check_item[1].as_integer<long>();
                    int whence = check_item[2].as_integer<int>();
                    auto i = item - item_range.cbegin();
                    REQUIRE(int_offset == magic->checks[i].offset.count);
                    REQUIRE(whence == (magic->checks[i].offset.from_start ? 0 : 2));
                }
            }
        }
    }
}