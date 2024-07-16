#pragma once

#include <vector>
#include <string>
#include <map>

#include <boost/outcome.hpp>

template <typename T>
using expected = boost::outcome_v2::result<T, std::string>;

using Binary = std::vector<uint8_t>;

enum class CompareType {
    Eq, EqUpper, Ne, Gt, Lt, And, Xor, Or, Nor
};

struct magic {
    struct check {
        CompareType compare_type;
        unsigned long long offset;
        Binary value;

        bool compare(Binary const& data) const;
    };
    std::vector<check> checks;
    std::string description;
    std::map<std::string, std::string> extensions;
    std::string pattern;
    std::vector<std::string> tags;

    size_t get_pattern_length(bool only_significant = false) const;
};

typedef std::vector<magic> Magics;

class SignatureUtil {
    Magics magics;

public:
    [[nodiscard]] expected<Magics> readMagics(std::string_view path);
};
