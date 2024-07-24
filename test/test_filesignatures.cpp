
#include <boost/filesystem/operations.hpp>
#include <jsoncons/json.hpp>
#include <fstream>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>

#include "filesignatures.h"

namespace {
    template <typename ItemType>
    struct DataGenerator : public Catch::Generators::IGenerator<ItemType> {
        jsoncons::json data;
        jsoncons::json::const_array_iterator current_ptr;
        ItemType current;

        DataGenerator(std::string_view path) : data(std::vector<int>()) {
            std::ifstream is(path.data());

            if (is.fail())
                throw std::runtime_error(std::string("Error: bad path ") + std::string(path));

            data = jsoncons::json(jsoncons::json::parse(is));
            current_ptr = data.array_range().cbegin();
        }

        ItemType const& get() const override {
            return current;
        }

        bool next() override {
            current_ptr++;
            if (current_ptr == data.array_range().end())
                return false;

            current = ItemType(*current_ptr);
            return true;
        }
    };

    struct LengthItem {
        std::string pattern;
        size_t len;
        LengthItem() = default;
        LengthItem(jsoncons::json const& json) :pattern(json.at(0).as<std::string>()), len(json.at(1).as<int>()) {}
    };
    using LengthDataGenerator = DataGenerator<LengthItem>;

    struct TestSignItem {
        Binary  binary;
        TestSignItem() = default;
        TestSignItem(jsoncons::json const& json) {
            auto char2uint8 = [](char input) -> uint8_t {
                if (input >= '0' && input <= '9')
                    return input - '0';
                if (input >= 'A' && input <= 'F')
                    return input - 'A' + 10;
                if (input >= 'a' && input <= 'f')
                    return input - 'a' + 10;
                return 0;
                };
            auto str2bin = [char2uint8](const std::string& src) -> Binary {
                Binary dst(src.length() / 2);
                auto di = dst.begin();
                for (size_t i = 0; i < src.length(); i += 2) {
                    *di++ = (char2uint8(src[i]) * 16 + char2uint8(src[i + 1]));
                }
                return dst;
                };
            binary = str2bin(json["data"].as_string());
        }
    };
    using TestSignDataGenerator = DataGenerator<TestSignItem>;
}

TEST_CASE("Compare with verified data", "[testSignatures]") {
    auto generator = TestSignDataGenerator("test/data/test_signatures.json");
    FileSigAnalyzer file_sig_analyzer;

    while (generator.next()) {
        auto data = generator.get();
        auto filename = boost::filesystem::unique_path().string();
        {
            std::fstream s{ filename, s.binary | s.trunc | s.out };
            s.write(reinterpret_cast<char*>(&data.binary[0]), data.binary.size());
        }
        std::filesystem::directory_entry de(filename);
        Magic result;
        auto ok = file_sig_analyzer.get_signature(de, result);
        //REQUIRE();
    }
}

TEST_CASE("Compare with verified data", "[get_pattern_length]") {
    auto generator = LengthDataGenerator("test/data/pattern_lengths.json");
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