#pragma once

#include <hasher/api.h>

#include "hex.h"
#include "llamaduck.h"

struct HashRec {
  void set(SFHASH_HashValues h, uint64_t metaAddr) {
    MetaAddr = metaAddr;
    MD5 = hexEncode(h.Md5, h.Md5 + sizeof(h.Md5));
    SHA1 = hexEncode(h.Sha1, h.Sha1 + sizeof(h.Sha1));
    SHA256 = hexEncode(h.Sha2_256, h.Sha2_256 + sizeof(h.Sha2_256));
    Blake3 = hexEncode(h.Blake3, h.Blake3 + sizeof(h.Blake3));
    Ssdeep = hexEncode(h.Fuzzy, h.Fuzzy + sizeof(h.Fuzzy));
  }

  static constexpr auto ColNames = {"MetaAddr",
                                    "MD5",
                                    "SHA1",
                                    "SHA256",
                                    "Blake3",
                                    "Ssdeep"};

  uint64_t MetaAddr;

  std::string MD5;
  std::string SHA1;
  std::string SHA256;
  std::string Blake3;
  std::string Ssdeep;
};

using HashBatch = DBBatch<HashRec>;

