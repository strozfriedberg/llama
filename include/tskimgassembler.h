#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "evidencerec.h"

class TskImgAssembler {
public:
  void addImage(const std::string& name, const std::string& path,
                const std::string& type, const std::string& description,
                uint64_t size, uint64_t sectorSize,
                const std::string& verificationHash);

  void addVolumeSystem(const std::string& type, const std::string& description,
                       uint64_t blockSize);

  void addVolume(uint64_t addr, uint64_t tableNum,
                 const std::string& description, const std::string& flags,
                 uint64_t numBlocks, uint64_t slotNum, uint64_t startBlock);

  void addFileSystem(uint64_t byteOffset, const std::string& type,
                     uint64_t blockSize, uint64_t blockCount,
                     uint64_t deviceBlockSize, const std::string& blockName,
                     bool littleEndian,
                     uint64_t firstBlock, uint64_t firstInum,
                     uint64_t lastBlock, uint64_t lastInum,
                     const std::string& flags, const std::string& fsID,
                     uint64_t journalInum, uint64_t rootInum,
                     uint64_t numInums);

  void setCurrentRootInodeId(const std::string& id);

  const EvidenceFileRec& evidenceFile() const { return Evidence; }
  const std::vector<VolumeRec>& volumes() const { return Volumes; }
  const std::vector<FilesystemRec>& filesystems() const { return Filesystems; }

  std::string currentEvidenceFileName() const { return Evidence.Name; }
  uint64_t currentByteOffset() const { return CurByteOffset; }
  uint32_t fsIndex() const { return FsIdx; }

private:
  enum { INIT, IMG, IMG_FS, VS, VOL, VOL_FS } State = INIT;

  EvidenceFileRec Evidence;
  std::vector<VolumeRec> Volumes;
  std::vector<FilesystemRec> Filesystems;

  // Current volume system context (stamped onto volumes)
  std::string CurVsType;
  std::string CurVsDescription;
  uint64_t CurVsBlockSize = 0;

  // Current volume context (stamped onto filesystems)
  uint64_t CurVolAddr = 0;
  uint64_t CurVolTableNum = 0;

  // Current filesystem tracking
  uint64_t CurByteOffset = 0;
  uint32_t FsIdx = 0;
};
