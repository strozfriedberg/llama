// file_signatures.cpp
//

#include <iostream>
#include <string>
#include <map>
#include <fstream>
#include <string_view>
#include <cctype> 
#include <exception>

#include <boost/algorithm/string.hpp>
#include <jsoncons/json.hpp>

#include "filesignatures.h"

auto makeUnexpected(const std::string& s) {
    return boost::outcome_v2::failure(s);
}

expected<CompareType> parse_compare_type(std::string_view s) {
    static const std::map<std::string_view, CompareType> dict = {
        {"=", CompareType::Eq},
        {"=/c", CompareType::EqUpper },
        { "!", CompareType::Ne },
        { ">", CompareType::Gt },
        { "<", CompareType::Lt },
        { "&", CompareType::And },
        { "^", CompareType::Xor },
        { "OR", CompareType::Or },
        { "NOR", CompareType::Nor }
    };

    if (auto val = dict.find(s); val != dict.end()) {
        return val->second;
    }

    return makeUnexpected(std::string("Unknown compare_type: ") + std::string(s));
}

bool magic::check::compare(Binary const& expected_value) {
    if (this->value.size() != expected_value.size())
        return false;

    switch (this->compare_type) {
        case CompareType::Eq:
            return this->value == expected_value;
        case CompareType::EqUpper:
            return boost::iequals(value, expected_value);
        case CompareType::Ne:
            return this->value != expected_value;
        case CompareType::Gt:
            return this->value > expected_value;
        case CompareType::Lt:
            return this->value < expected_value;
        case CompareType::And:
            return this->value == expected_value; // '&': operator.eq,
        case CompareType::Xor:
            throw std::runtime_error("compare: CompareType::Xor is not implemented yet");
        case CompareType::Or: {
            uint8_t result = 0;
            for (std::size_t i = 0; i < this->value.size(); ++i) {
                result |= this->value[i] | expected_value[i];
            }
            return result != 0;
        }
        case CompareType::Nor: {
            uint8_t result = 0;
            for (std::size_t i = 0; i < this->value.size(); ++i) {
                result |= this->value[i] | expected_value[i];
            }
            return result == 0;
        }
        default:
            throw std::range_error("compare: impossible");
    }
    return false;
}

expected<Magics> SignatureUtil::readMagics(std::string_view path) {
    try {
        std::ifstream is(path.data());
        if (is.fail())
            return makeUnexpected(std::string("Error: bad path ") + std::string(path));

        auto json(jsoncons::json::parse(is));
        auto startsWith = [](const std::string& s, const std::string& prefix) -> bool {
            return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
            };
        auto parseOffset = [&startsWith](std::string s) -> unsigned long long {
            std::stringstream ss;
            if (startsWith(s, "0x") || startsWith(s, "0X")) {
                ss << std::hex << s.substr(2);
            }
            else {
                ss << std::dec << s;
            }
            unsigned long long v;
            ss >> v;
            return v;
            };
        auto char2uint8 = [](char input) -> uint8_t {
            if (input >= '0' && input <= '9')
                return input - '0';
            if (input >= 'A' && input <= 'F')
                return input - 'A' + 10;
            if (input >= 'a' && input <= 'f')
                return input - 'a' + 10;
            return 0;
            };

        // accept 0xABCD (or 1234), return [0xAB, 0xCD] (or [12, 34])
        auto str2bin = [char2uint8, startsWith](const std::string& src) -> Binary {
            bool hex = startsWith(src, "0x") || startsWith(src, "0X");
            Binary dst(src.length() / 2 - hex);
            auto di = dst.begin();
            for (size_t i = hex * 2; i < src.length(); i += 2) {
                *di++ = (char2uint8(src[i]) * (hex ? 16 : 10) + char2uint8(src[i + 1]));
            }
            return dst;
            };

        Magics magics;
        for (const auto& magic_json : json.array_range()) {
            magic m;
            if (magic_json.contains("checks")) {
                for (const auto& check : magic_json["checks"].array_range()) {
                    if (auto compare_type = parse_compare_type(check["compare_type"].as_string()))
                        m.checks.push_back(magic::check{
                          compare_type.value(),
                          parseOffset(check["offset"].as_string()),
                          str2bin(check["value"].as_string()) });
                    else {
                        return makeUnexpected(compare_type.error());
                    }
                }
            }
            if (magic_json.contains("pattern")) {
                m.pattern = magic_json["pattern"].as_string();
            }
            if (magic_json.contains("description")) {
                m.description = magic_json["description"].as_string();
            }
            if (magic_json.contains("tags")) {
                for (const auto& tag : magic_json["tags"].array_range()) {
                    m.tags.push_back(tag.as_string());
                }
            }
            if (magic_json.contains("extensions")) {
                for (const auto& ext : magic_json["extensions"].object_range()) {
                    m.extensions[ext.key()] = ext.value().as_string();
                }
            }
            magics.push_back(m);
        }

        return magics;
    }
    catch (std::exception& e) {
        return makeUnexpected(e.what());
    }
}

/*
std::string dump(Binary const& data) {
    std::string result;
    result.reserve(data.size() * 2);
    for (auto v : data) {
        auto tmp = std::format("{:02x}", v);
        result.push_back(tmp[0]);
        result.push_back(tmp[1]);
    }

    return result;
}

using namespace boost::iostreams;

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << argv[0] << " <path to magic.json> <path to file_obj>\n";
        return 3;
    }

    SignatureUtil   su;
    auto result = su.readMagics(argv[1]);
    mapped_file     mmap(argv[2], mapped_file::readonly);
    auto            data = mmap.const_data();

    if (result) {
        auto magics = result.value();
        std::cout << "Ok!\n";
        for (auto magic : magics) {
            for (auto check : magic.checks) {
                Binary expected_value;

                if (check.value.starts_with("0x")) {
                    if (auto value = unhexlify(check.value.substr(2))) {
                        expected_value = value.value();
                    }
                    else {
                        std::cout << "Error: " << value.error() << "\n";
                        return 3;
                    }
                }
                else {
                    expected_value = Binary(check.value.begin(), check.value.end());
                }

                std::println("{}, {:x}, '{}'",
                    magic_enum::enum_name(check.compare_type), check.offset, dump(expected_value));

                auto offset = check.offset;
                auto compare_type = check.compare_type;
                auto size = (compare_type == CompareType::Or || compare_type == CompareType::Nor) ? 1 : expected_value.size();

                if (offset + size > mmap.size()) {
                    std::cerr << "Error: offset + size (" << offset + size << ") > filesize(" << mmap.size() << "\n";
                    return 4;
                }

                auto value = Binary(data + offset, data + offset + size);
                //TODO: pre_process
                if (compare(value, expected_value, compare_type)) {
                    std::println("value '{}' {} expected_value '{}' -> true",
                        dump(value), magic_enum::enum_name(check.compare_type), dump(expected_value));
                }
            }
        }
    }
    else {
        std::cerr << "Error: " << result.error() << "\n";
        return 3;
    }
    return 0;
}
*/