#pragma once

#include <array>
#include <cstdint>
#include <string>

struct Inode {

  static constexpr auto ColNames = {"Id",
                                    "Type",
                                    "Flags",
                                    "Addr",
                                    "EvidenceFileName",
                                    "ByteOffset",
                                    "Filesize",
                                    "Uid",
                                    "Gid",
                                    "LinkTarget",
                                    "NumLinks",
                                    "SeqNum",
                                    "Created",
                                    "Accessed",
                                    "Modified",
                                    "Metadata"};

  std::array<uint8_t, 32> Id{};

  std::string Type;
  std::string Flags;

  uint64_t Addr;
  std::string EvidenceFileName;
  uint64_t ByteOffset;
  uint64_t Filesize;
  uint64_t Uid;
  uint64_t Gid;

  std::string LinkTarget;
  uint64_t    NumLinks;
  uint64_t    SeqNum;

  std::string Created;
  std::string Accessed;
  std::string Modified;
  std::string Metadata;
};

