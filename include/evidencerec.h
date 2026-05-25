#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "llamaduck.h"

struct EvidenceFileRec {
  static constexpr auto ColNames = {"Name",
                                    "Path",
                                    "ImageType",
                                    "ImageDescription",
                                    "ImageSize",
                                    "SectorSize",
                                    "VerificationHash"};

  std::string Name;
  std::string Path;
  std::string ImageType;
  std::string ImageDescription;
  uint64_t    ImageSize;
  uint64_t    SectorSize;
  std::string VerificationHash;
};

using EvidenceFileBatch = DBBatch<EvidenceFileRec>;

struct VolumeRec {
  static constexpr auto ColNames = {"EvidenceFileName",
                                    "Addr",
                                    "TableNum",
                                    "Description",
                                    "Flags",
                                    "NumBlocks",
                                    "SlotNum",
                                    "StartBlock",
                                    "VsType",
                                    "VsDescription",
                                    "VsBlockSize"};

  std::string EvidenceFileName;
  uint64_t    Addr;
  uint64_t    TableNum;
  std::string Description;
  std::string Flags;
  uint64_t    NumBlocks;
  uint64_t    SlotNum;
  uint64_t    StartBlock;
  std::string VsType;
  std::string VsDescription;
  uint64_t    VsBlockSize;
};

using VolumeBatch = DBBatch<VolumeRec>;

struct FilesystemRec {
  static constexpr auto ColNames = {"EvidenceFileName",
                                    "ByteOffset",
                                    "VolumeAddr",
                                    "VolumeTableNum",
                                    "Type",
                                    "BlockSize",
                                    "BlockCount",
                                    "DeviceBlockSize",
                                    "BlockName",
                                    "LittleEndian",
                                    "FirstBlock",
                                    "FirstInum",
                                    "LastBlock",
                                    "LastInum",
                                    "Flags",
                                    "FsID",
                                    "JournalInum",
                                    "RootInum",
                                    "NumInums",
                                    "RootDirentId",
                                    "RootInodeId"};

  std::string EvidenceFileName;
  uint64_t    ByteOffset;
  uint64_t    VolumeAddr;
  uint64_t    VolumeTableNum;
  std::string Type;
  uint64_t    BlockSize;
  uint64_t    BlockCount;
  uint64_t    DeviceBlockSize;
  std::string BlockName;
  uint64_t    LittleEndian;
  uint64_t    FirstBlock;
  uint64_t    FirstInum;
  uint64_t    LastBlock;
  uint64_t    LastInum;
  std::string Flags;
  std::string FsID;
  uint64_t    JournalInum;
  uint64_t    RootInum;
  uint64_t    NumInums;
  std::array<uint8_t, 32> RootDirentId;
  std::array<uint8_t, 32> RootInodeId;
};

using FilesystemBatch = DBBatch<FilesystemRec>;
