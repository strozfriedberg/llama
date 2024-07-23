#pragma once

#include <vector>
#include <string>
#include <map>

#include <lightgrep/api.h>
#include <boost/outcome.hpp>
#include <boost/range/iterator_range.hpp>

template <typename T>
using expected = boost::outcome_v2::result<T, std::string>;

using Binary = std::vector<uint8_t>;

inline auto makeUnexpected(const std::string& s) {
    return boost::outcome_v2::failure(s);
}

enum class CompareType {
    Eq, EqUpper, Ne, Gt, Lt, And, Xor, Or, Nor
};

struct Offset {
    uint64_t    count;
    bool        from_start;
};

struct magic {
    struct check {
        CompareType compare_type;
        Offset      offset;
        Binary      value;
        Binary      pre_process;

        bool compare(Binary const& data) const;
    };
    std::vector<check>                  checks;
    std::string                         description;
    std::string                         id;
    std::map<std::string, std::string>  extensions;
    std::string                         pattern;
    bool                                fixed_string;
    bool                                case_insensetive;
    std::vector<std::string>            encodings;
    std::vector<std::string>            tags;

    size_t get_pattern_length(bool only_significant = false) const;
};

size_t get_pattern_length(std::string const& pattern, bool only_significant);

typedef std::vector<std::shared_ptr<magic>> Magics;

class SignatureUtil {
    Magics magics;

public:
    [[nodiscard]] expected<Magics> readMagics(std::string_view path);
};

typedef boost::iterator_range<const uint8_t*> MemoryRegion;

class LightGrep {
    LG_HPROGRAM _prog;

public:
    LightGrep();
    ~LightGrep();
    expected<size_t> setup(Magics const& m);
    expected<bool> search(MemoryRegion const& region, void* user_data, LG_HITCALLBACK_FN callback_fn) const;
    LG_HPROGRAM get_lg_prog() const{ return _prog; }
};
