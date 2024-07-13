// file_signatures.cpp
//

#include <iostream>
#include <string>
#include <map>
#include <fstream>
#include <vector>
#include <string_view>
#include <expected>
#include <format>
#include <print>
#include <cctype> 

#include <boost/algorithm/string.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <jsoncons/json.hpp>
#include <magic_enum/magic_enum.hpp>

typedef std::vector<uint8_t> Binary;

enum class CompareType {
    Eq, EqUpper, Ne, Gt, Lt, And, Xor, Or, Nor
};

std::expected<CompareType, std::string>  parse_compare_type(std::string_view s) {
    static const std::map<std::string_view, CompareType>    dict = {
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

    if (auto val = dict.find(s); val != dict.end())
        return val->second;

    return std::unexpected(std::format("Unknown compare_type: '{}'", s));
}

bool compare(Binary const & value, Binary const & expected_value, CompareType op) {
    if (value.size() != expected_value.size())
        return false;

    switch (op) {
        case CompareType::Eq:
            return value == expected_value;
        case CompareType::EqUpper:
            return boost::iequals(value, expected_value);
        case CompareType::Ne:
            return value != expected_value;
        case CompareType::Gt:
            return value > expected_value;
        case CompareType::Lt:
            return value < expected_value;
        case CompareType::And:
            return value == expected_value; // '&': operator.eq,
        case CompareType::Xor:
            throw std::exception("compare: CompareType::Xor is not implemented yet");
        case CompareType::Or: {
            uint8_t result = 0;
            for (auto i = 0; i < value.size(); ++i) {
                result |= value[i] | expected_value[i];
            }
            return result != 0;
        }
        case CompareType::Nor: {
            uint8_t result = 0;
            for (auto i = 0; i < value.size(); ++i) {
                result |= value[i] | expected_value[i];
            }
            return result == 0;
        }
        default:
            throw std::exception("compare: impossible");
    }
}

struct magic {
    struct check {
        CompareType compare_type;
        unsigned long long offset;
        std::string value;
    };
    std::vector<check> checks;
    std::string description;
    std::map<std::string, std::string> extensions;
    std::string pattern;
    std::vector<std::string> tags;
};

typedef std::vector<magic> Magics;

class SignatureUtil {
    Magics magics;

public:
    [[nodiscard]] std::expected<Magics, std::string> readMagics(std::string_view path);
};

std::expected<Magics, std::string> SignatureUtil::readMagics(std::string_view path) {
    try {
        std::ifstream is(path.data());
        if (is.fail())
            return std::unexpected(std::format("Error: bad path '{}'", path));

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

        Magics magics;
        for (const auto& magic_json : json.array_range()) {
            magic m;
            if (magic_json.contains("checks")) {
                for (const auto& check : magic_json["checks"].array_range()) {
                    if (auto compare_type = parse_compare_type(check["compare_type"].as_string()))
                        m.checks.push_back(magic::check{
                          compare_type.value(),
                          parseOffset(check["offset"].as_string()),
                          check["value"].as_string() });
                    else
                        return std::unexpected(compare_type.error());
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
    catch (std::exception e) {
        return std::unexpected(std::format("Error: {}", e.what()));
    }
}


int to_int(int c) {
    if (not isxdigit(c)) return -1; // error: non-hexadecimal digit found
    if (isdigit(c)) return c - '0';
    if (isupper(c)) c = tolower(c);
    return c - 'a' + 10;
}

std::expected<Binary, std::string>
unhexlify(std::string_view input) {
    Binary result;

    result.reserve(input.length() / 2);

    for (auto it = input.begin(); it != input.end();) {
        int top = to_int(*it++);
        int bot = to_int(*it++);
        if (top == -1 or bot == -1)
            return std::unexpected(std::format("unhexlify: bad input '{}'", input));
        result.push_back((top << 4) + bot);
    }

    return result;
}

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

