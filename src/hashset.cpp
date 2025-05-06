#include "hashset.h"
#include "readseek_impl.h"
#include "throw.h"
#include "hex.h"

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
    Hashset(getHashset(HashsetRegion, path)) {
  setSupportedHashAlg();
}

LlamaHashset::~LlamaHashset() {
  sfhash_destroy_hashset(Hashset);
}

std::string LlamaHashset::getHash() const {
  auto hash = sfhash_hashset_sha2_256(Hashset);
  return hexEncode(hash, 32);
}

void LlamaHashset::setSupportedHashAlg() {
  for (std::vector<SFHASH_HashAlgorithm>::const_iterator it = searchedHashAlgs.begin(); SupportedHashAlgIdx == -1 && it != searchedHashAlgs.end(); it++) {
    SupportedHashAlg = *it;
    SupportedHashAlgIdx = sfhash_hashset_index_for_type(Hashset, SupportedHashAlg);
  }
  THROW_IF(SupportedHashAlgIdx == -1, "No supported hash algorithm found in hashset.");
}

bool LlamaHashset::lookup(const uint8_t* hash) {
  return sfhash_hashset_lookup(Hashset, SupportedHashAlgIdx, hash);
}

bool LlamaHashset::lookup(const SFHASH_HashValues& h) {
  const uint8_t* hash;
  switch (SupportedHashAlg) {
    case SFHASH_MD5: hash = h.Md5; break;
    case SFHASH_SHA_1: hash = h.Sha1; break;
    case SFHASH_SHA_2_256: hash = h.Sha2_256; break;
    case SFHASH_BLAKE3: hash = h.Blake3; break;
    default: THROW("Supported hash alg value for hset is not actually supported. This shouldn't happen.");
  };
  return lookup(hash);
}
