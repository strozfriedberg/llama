#include <vector>

#include "hasher/api.h"

#include "boost/interprocess/file_mapping.hpp"
#include "boost/interprocess/mapped_region.hpp"

namespace bip = boost::interprocess;

struct LlamaHashset {
  LlamaHashset(const char* path);
  ~LlamaHashset();

  // Returns true if the given hash is contained in the hash set.
  bool lookup(const uint8_t* hash);
  bool lookup(const SFHASH_HashValues& h);

private:
  int getSupportedHashAlgIdx();

  // The hashset file is memory-mapped, so we want to ensure the lifetime of the file mapping and mapped region
  bip::file_mapping HashsetMapping;
  bip::mapped_region HashsetRegion;

  SFHASH_Hashset* Hashset;
  int SupportedHashAlgIdx;
};
