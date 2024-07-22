
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
    auto generator = VerifiedDataGenerator("test/data/calculated_by_python.json");
    while (generator.next()) {
        auto data = generator.get();
        REQUIRE(::get_pattern_length(data.pattern, true) == data.len);
    }
}