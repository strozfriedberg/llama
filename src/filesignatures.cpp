// file_signatures.cpp
//

#include "filesignatures.h"

#include <iostream>
#include <string>
#include <map>
#include <fstream>
#include <string_view>
#include <cctype>
#include <exception>
#include <algorithm>

#include <boost/algorithm/string.hpp>

#include <jsoncons/json.hpp>

#include "util.h"

LightGrep::LightGrep()
{}

LightGrep::~LightGrep() {
    if (_prog) {
        lg_destroy_program(_prog);
    }
}

// return max_read
expected<size_t> LightGrep::setup(Magics const& m) {
    using namespace boost;

    size_t max_read = 0;
    try {
        LG_Error* err = 0;
        LG_HFSM fsm = lg_create_fsm(0, 0);
        destroy_guard fsm_guard([&fsm]() { lg_destroy_fsm(fsm); });

        for (std::size_t i = 0; i < m.size(); i++) {

            auto& p = m[i];

            if (p->pattern.empty()) {
                continue;
            }

            LG_KeyOptions opt = { p->fixed_string, p->case_insensetive, false };

            auto pattern = lg_create_pattern();
            destroy_guard pattern_guard([&pattern]() { lg_destroy_pattern(pattern); });
            if (!lg_parse_pattern(pattern, p->pattern.c_str(), &opt, &err)) {
                if (err) {
                    auto r = makeUnexpected(err->Message);
                    lg_free_error(err);
                    return r;
                }
            }

            auto pattern_len = p->get_pattern_length(false);
            if (pattern_len > max_read) {
                max_read = pattern_len;
            }

            for (auto const& encoding : p->encodings) {
                if (!lg_add_pattern(fsm, pattern, encoding.c_str(), i, &err)) {
                    if (err) {
                        auto r = makeUnexpected(err->Message);
                        lg_free_error(err);
                        return r;
                    }
                }
            }
        }

        LG_ProgramOptions opts = { 0 };
        if (!(_prog = lg_create_program(fsm, &opts))) {
            return makeUnexpected("lg_create_program() failed");
        }
    }
    catch (std::exception const& ex) {
        return makeUnexpected(ex.what());
    }

    return max_read;
}

expected<bool> LightGrep::search(MemoryRegion const& region, void* user_data, LG_HITCALLBACK_FN callback_fn) const {
    try {
        LG_ContextOptions ctxOpts = { 0, 0 };
        LG_HCONTEXT searcher = lg_create_context(_prog, &ctxOpts);
        lg_reset_context(searcher);
        lg_starts_with(searcher, (const char*)boost::begin(region), (const char*)boost::end(region), 0, user_data, callback_fn);
        lg_destroy_context(searcher);
    }
    catch (std::exception const& ex) {
        return makeUnexpected(ex.what());
    }
    return true;
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

bool iequals(char lhs, char rhs) {
    std::locale loc;
    return std::toupper(lhs, loc) == std::toupper(rhs, loc);
}

bool magic::check::compare(Binary const& data) const {
    if (data.size() < value.size() + offset)
        return false;

    auto expected_value = Binary(&data[offset], &data[offset + value.size()]);

    switch (compare_type) {
        case CompareType::Eq:
            return value == expected_value;
        case CompareType::EqUpper:
            return boost::equals(value, expected_value, iequals);
        case CompareType::Ne:
            return value != expected_value;
        case CompareType::Gt:
            return value > expected_value;
        case CompareType::Lt:
            return value < expected_value;
        case CompareType::And:
            return value == expected_value; // '&': operator.eq,
        case CompareType::Xor:
            throw std::runtime_error("compare: CompareType::Xor is not implemented yet");
        case CompareType::Or: {
            uint8_t result = 0;
            for (std::size_t i = 0; i < value.size(); ++i) {
                result |= value[i] | expected_value[i];
            }
            return result != 0;
        }
        case CompareType::Nor: {
            uint8_t result = 0;
            for (std::size_t i = 0; i < value.size(); ++i) {
                result |= value[i] | expected_value[i];
            }
            return result == 0;
        }
        default:
            throw std::range_error("compare: impossible OP " + std::to_string((int)compare_type));
    }
    return false;
}

size_t get_pattern_length(std::string const& pattern, bool only_significant) {
    std::size_t i = 0;
    size_t count = 0;
    char prev_c = 0;

    while (i < pattern.size()) {
        auto c = pattern[i];
        if (c == '\\') {
            if (pattern[i + 1] == 'x') {
                count += 1;
                i += 4;
            } else { // \\u0000
                count += 1;
                i += 6;
            }
        }
        else if (c == '{') {
            auto j = pattern.find('}', i + 1);
            auto i2 = pattern.find(',', i + 1);

            if (i2 < j)
                i = i2;

            auto x = std::stoi(pattern.substr(i + 1, j));
            i = j + 1;
            count += only_significant && prev_c == '.' ? 0 : x;
        }
        else if (c == '[') {
            auto j = pattern.find(']', i + 1);
            i = j + 1;
            count += 1;
        }
        else if (c == '.') {
            i += 1;
            count += only_significant ? 0 : 1;
        }
        else {
            count += 1;
            i += 1;
        }
        prev_c = c;
    }

    return count * 4;
}

size_t magic::get_pattern_length(bool only_significant) const {
    return ::get_pattern_length(pattern, only_significant);
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
            m.fixed_string = magic_json.contains("fixed_string") ? magic_json["fixed_string"].as_bool() : false;
            m.case_insensetive = magic_json.contains("case_insensitive") ? magic_json["case_insensitive"].as_bool() : false;

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
            if (magic_json.contains("encoding")) {
                boost::split(m.encodings, magic_json["encoding"].as_string(), boost::is_any_of(","));
            }
            else {
                m.encodings.push_back("ISO-8859-1");
            }
            magics.push_back(std::make_shared<magic>(m));
        }

        return magics;
    }
    catch (std::exception& e) {
        return makeUnexpected(e.what());
    }
}
