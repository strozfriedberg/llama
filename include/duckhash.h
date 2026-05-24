#pragma once

#include <hasher/api.h>

#include "hex.h"
#include "llamaduck.h"

struct HashRec {
  void set(SFHASH_HashValues h, std::string inodeId, uint64_t hashAlgs) {
    InodeId = std::move(inodeId);
    MD5 = hashAlgs & SFHASH_MD5 ? hexEncode(h.Md5, h.Md5 + sizeof(h.Md5)) : "";
    SHA1 = hashAlgs & SFHASH_SHA_1 ? hexEncode(h.Sha1, h.Sha1 + sizeof(h.Sha1)): "";
    SHA256 = hashAlgs & SFHASH_SHA_2_256 ? hexEncode(h.Sha2_256, h.Sha2_256 + sizeof(h.Sha2_256)): "";
    Blake3 = hashAlgs & SFHASH_BLAKE3 ? hexEncode(h.Blake3, h.Blake3 + sizeof(h.Blake3)): "";
    Ssdeep = hashAlgs & SFHASH_FUZZY ? hexEncode(h.Fuzzy, h.Fuzzy + sizeof(h.Fuzzy)): "";
  }

  static constexpr auto ColNames = {"InodeId",
                                    "MD5",
                                    "SHA1",
                                    "SHA256",
                                    "Blake3",
                                    "Ssdeep"};

  std::string InodeId;

  std::string MD5;
  std::string SHA1;
  std::string SHA256;
  std::string Blake3;
  std::string Ssdeep;
};

using HashBatch = DBBatch<HashRec>;
