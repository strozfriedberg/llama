#include "hashset.h"
#include "readseek_impl.h"
#include "throw.h"

#include <lightgrep/api.h>

namespace {
  const std::vector<SFHASH_HashAlgorithm> searchedHashAlgs{SFHASH_MD5, SFHASH_SHA_1, SFHASH_SHA_2_256, SFHASH_BLAKE3};

  SFHASH_Hashset* getHashset(const bip::mapped_region& mr, const char* path) {
    uint8_t* beg = reinterpret_cast<uint8_t*>(mr.get_address());
    uint8_t* end = beg + mr.get_size();

    SFHASH_Error* err = nullptr;
    SFHASH_Hashset* hset = sfhash_load_hashset(beg, end, &err);
    THROW_IF(err, "Failed to load hashset " << path << " due to " << err->message);

    return hset;
  }
}
LlamaHashset::LlamaHashset(const char* path) 
  : HashsetMapping(path, bip::read_only),
    HashsetRegion(HashsetMapping, bip::read_only),
    Hashset(getHashset(HashsetRegion, path)),
    SupportedHashAlgIdx(getSupportedHashAlgIdx()) {}

LlamaHashset::~LlamaHashset() {
  sfhash_destroy_hashset(Hashset);
}

int LlamaHashset::getSupportedHashAlgIdx() {
  int idx = -1;
  for (const SFHASH_HashAlgorithm alg: searchedHashAlgs) {
    idx = sfhash_hashset_index_for_type(Hashset, alg);
    if (idx >= 0) {
      return idx;
    }
  }
  return idx;
}

bool LlamaHashset::lookup(const uint8_t* hash) {
  if (SupportedHashAlgIdx >= 0) {
    // This means that our SupportedHashAlgIdx is initialized and valid
    return sfhash_hashset_lookup(Hashset, SupportedHashAlgIdx, hash);
  }
  // If we get here, that means there was no supported hash algorithm in the hset
  return false;
}
