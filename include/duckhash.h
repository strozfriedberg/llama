#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>

#include <hasher/api.h>

#include "llamaduck.h"

namespace duckhash_detail {

template<size_t N>
std::array<uint8_t, N> toArray(const uint8_t* src) {
  std::array<uint8_t, N> dst;
  std::memcpy(dst.data(), src, N);
  return dst;
}

inline std::string ssdeepText(const uint8_t* fuzzy) {
  // ssdeep is a NUL-terminated C string within its fixed Fuzzy buffer.
  return std::string(reinterpret_cast<const char*>(fuzzy));
}

} // namespace duckhash_detail

struct HashRec {
  static constexpr auto ColNames = {"InodeId", "MD5", "SHA1", "SHA256", "Blake3", "Ssdeep"};

  std::array<uint8_t, 32>                InodeId;          // FK to inode.Id, NOT NULL
  std::optional<std::array<uint8_t, 16>> MD5;
  std::optional<std::array<uint8_t, 20>> SHA1;
  std::optional<std::array<uint8_t, 32>> SHA256;
  std::optional<std::array<uint8_t, 32>> Blake3;
  std::string                            Ssdeep;            // "" when absent

  void set(const SFHASH_HashValues& h,
           const std::array<uint8_t, 32>& inodeId,
           uint64_t hashAlgs) {
    InodeId = inodeId;
    MD5    = (hashAlgs & SFHASH_MD5)       ? std::optional{duckhash_detail::toArray<16>(h.Md5)}      : std::nullopt;
    SHA1   = (hashAlgs & SFHASH_SHA_1)     ? std::optional{duckhash_detail::toArray<20>(h.Sha1)}     : std::nullopt;
    SHA256 = (hashAlgs & SFHASH_SHA_2_256) ? std::optional{duckhash_detail::toArray<32>(h.Sha2_256)} : std::nullopt;
    Blake3 = (hashAlgs & SFHASH_BLAKE3)    ? std::optional{duckhash_detail::toArray<32>(h.Blake3)}   : std::nullopt;
    Ssdeep = (hashAlgs & SFHASH_FUZZY)     ? duckhash_detail::ssdeepText(h.Fuzzy)                    : std::string{};
  }
};

using HashBatch = DBBatch<HashRec>;
