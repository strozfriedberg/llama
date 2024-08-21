#pragma once

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <duckdb.h>

#include "llamaduck.h"
#include "throw.h"

struct Dirent : public SchemaType<std::string, std::string, std::string, std::string, std::string, std::string, uint64_t, uint64_t, uint64_t, uint64_t>
{
  static constexpr auto ColNames = {"Id",
                                    "Path",
                                    "Name",
                                    "ShortName",
                                    "Type",
                                    "Flags",
                                    "MetaAddr",
                                    "ParentAddr",
                                    "MetaSeq",
                                    "ParentSeq"};

  std::string Id;
  std::string Path;
  std::string Name;
  std::string ShortName;

  std::string Type;
  std::string Flags;

  uint64_t MetaAddr;
  uint64_t ParentAddr;
  uint64_t MetaSeq;
  uint64_t ParentSeq;

  Dirent(const std::string& p = "",
         const std::string& n = "",
         const std::string& shrt = "",
         const std::string& type = "",
         const std::string& flags = "",
         uint64_t meta = 0,
         uint64_t parent = 0,
         uint64_t metaSeq = 0,
         uint64_t parentSeq = 0
  ):
    Path(p), Name(n), ShortName(shrt), Type(type), Flags(flags),
    MetaAddr(meta), ParentAddr(parent), MetaSeq(metaSeq), ParentSeq(parentSeq) {}

  bool operator==(const Dirent& other) const {
    return Id == other.Id &&
           Path == other.Path &&
           Name == other.Name &&
           ShortName == other.ShortName &&
           Type == other.Type &&
           Flags == other.Flags &&
           MetaAddr == other.MetaAddr &&
           ParentAddr == other.ParentAddr &&
           MetaSeq == other.MetaSeq &&
           ParentSeq == other.ParentSeq;
  }
};

struct DirentBatch {
  struct StrOffsets {
    size_t IdOffset,
           PathOffset,
           NameOffset,
           ShortOffset,
           TypeOffset,
           FlagsOffset;
  };

  struct Uint64Vals {
    uint64_t MetaAddr,
             ParentAddr,
             MetaSeq,
             ParentSeq;
  };

  size_t size() const { return Offsets.size(); }

  std::vector<char> Buf;
  std::vector<StrOffsets> Offsets;
  std::vector<Uint64Vals> Nums;

  static bool createTable(duckdb_connection& dbconn, const std::string& table);

  void add(const Dirent& dent) {
    size_t idSize = dent.Id.size() + 1;
    size_t pathSize = dent.Path.size() + 1;
    size_t nameSize = dent.Name.size() + 1;
    size_t shrtSize = dent.ShortName.size() + 1;
    size_t typeSize = dent.Type.size() + 1;
    size_t flagsSize = dent.Flags.size() + 1;
    size_t startOffset = Buf.size();
    size_t totalSize = idSize + pathSize + nameSize + shrtSize + typeSize + flagsSize;
    Buf.resize(startOffset + totalSize);
    StrOffsets offsets{startOffset,
                       startOffset + idSize,
                       startOffset + idSize + pathSize,
                       startOffset + idSize + pathSize + nameSize,
                       startOffset + idSize + pathSize + nameSize + shrtSize,
                       startOffset + idSize + pathSize + nameSize + shrtSize + typeSize};

    std::copy_n(dent.Id.begin(), idSize, Buf.begin() + offsets.IdOffset);
    std::copy_n(dent.Path.begin(), pathSize, Buf.begin() + offsets.PathOffset);
    std::copy_n(dent.Name.begin(), nameSize, Buf.begin() + offsets.NameOffset);
    std::copy_n(dent.ShortName.begin(), shrtSize, Buf.begin() + offsets.ShortOffset);
    std::copy_n(dent.Type.begin(), typeSize, Buf.begin() + offsets.TypeOffset);
    std::copy_n(dent.Flags.begin(), flagsSize, Buf.begin() + offsets.FlagsOffset);

    Offsets.push_back(offsets);
    Nums.push_back(Uint64Vals{dent.MetaAddr, dent.ParentAddr, dent.MetaSeq, dent.ParentSeq});
  }

  void copyToDB(duckdb_appender& appender);
};

