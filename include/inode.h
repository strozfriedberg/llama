#pragma once

#include <cstdint>
#include <string>

struct Inode {

  static constexpr auto ColNames = {"Id",
                                    "Type",
                                    "Flags",
                                    "Addr",
                                    "FsOffset",
                                    "Uid",
                                    "Gid",
                                    "LinkTarget",
                                    "NumLinks",
                                    "SeqNum",
                                    "Created",
                                    "Accessed",
                                    "Modified",
                                    "Metadata"};

  std::string Id;

  std::string Type;
  std::string Flags;

  uint64_t Addr;
  uint64_t FsOffset;
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

