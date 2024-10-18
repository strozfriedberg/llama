#pragma once

#include "llamaduck.h"

struct HashRec {

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

