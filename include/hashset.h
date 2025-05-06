#include <vector>

#include "hasher/api.h"

#include "boost/interprocess/file_mapping.hpp"
#include "boost/interprocess/mapped_region.hpp"

namespace bip = boost::interprocess;

struct LlamaHashset {
  LlamaHashset(const char* path);
  ~LlamaHashset();

  // Returns true if the given hash is contained in the hash set.
  bool lookup(const SFHASH_HashValues& h);

  // Returns the first known supported hash algorithm from the hashset.
  // This can be used to determine which hashes to calculate for files in the batch.
  SFHASH_HashAlgorithm supportedHashAlg() const { return SupportedHashAlg; }

  std::string getName() const { return sfhash_hashset_name(Hashset); }
  std::string getHash() const;

private:
  void setSupportedHashAlg();
  bool lookup(const uint8_t* hash);

  // The hashset file is memory-mapped, so we want to ensure the lifetime of the file mapping and mapped region
  bip::file_mapping HashsetMapping;
  bip::mapped_region HashsetRegion;

  SFHASH_Hashset* Hashset;
  int SupportedHashAlgIdx = -1;
  SFHASH_HashAlgorithm SupportedHashAlg;
};
