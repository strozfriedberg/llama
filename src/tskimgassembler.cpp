#include "tskimgassembler.h"
#include "throw.h"

void TskImgAssembler::addImage(const std::string& name, const std::string& path,
                                const std::string& type, const std::string& description,
                                uint64_t size, uint64_t sectorSize,
                                const std::string& verificationHash) {
  THROW_IF(State != INIT, "Attempted to add image in state " << State);
  Evidence.Name = name;
  Evidence.Path = path;
  Evidence.ImageType = type;
  Evidence.ImageDescription = description;
  Evidence.ImageSize = size;
  Evidence.SectorSize = sectorSize;
  Evidence.VerificationHash = verificationHash;
  State = IMG;
}

void TskImgAssembler::addVolumeSystem(const std::string& type,
                                       const std::string& description,
                                       uint64_t blockSize) {
  THROW_IF(State != IMG, "Attempted to add volume system in state " << State);
  CurVsType = type;
  CurVsDescription = description;
  CurVsBlockSize = blockSize;
  State = VS;
}

void TskImgAssembler::addVolume(uint64_t addr, uint64_t tableNum,
                                 const std::string& description,
                                 const std::string& flags,
                                 uint64_t numBlocks, uint64_t slotNum,
                                 uint64_t startBlock) {
  switch (State) {
  case VS:
  case VOL:
  case VOL_FS:
    break;
  default:
    THROW("Attempted to add volume in state " << State);
  }

  VolumeRec vol;
  vol.EvidenceFileName = Evidence.Name;
  vol.Addr = addr;
  vol.TableNum = tableNum;
  vol.Description = description;
  vol.Flags = flags;
  vol.NumBlocks = numBlocks;
  vol.SlotNum = slotNum;
  vol.StartBlock = startBlock;
  vol.VsType = CurVsType;
  vol.VsDescription = CurVsDescription;
  vol.VsBlockSize = CurVsBlockSize;
  Volumes.push_back(std::move(vol));

  CurVolAddr = addr;
  CurVolTableNum = tableNum;
  State = VOL;
}

void TskImgAssembler::addFileSystem(uint64_t byteOffset, const std::string& type,
                                     uint64_t blockSize, uint64_t blockCount,
                                     uint64_t deviceBlockSize, const std::string& blockName,
                                     bool littleEndian,
                                     uint64_t firstBlock, uint64_t firstInum,
                                     uint64_t lastBlock, uint64_t lastInum,
                                     const std::string& flags, const std::string& fsID,
                                     uint64_t journalInum, uint64_t rootInum,
                                     uint64_t numInums) {
  FilesystemRec fs;
  fs.EvidenceFileName = Evidence.Name;
  fs.ByteOffset = byteOffset;
  fs.Type = type;
  fs.BlockSize = blockSize;
  fs.BlockCount = blockCount;
  fs.DeviceBlockSize = deviceBlockSize;
  fs.BlockName = blockName;
  fs.LittleEndian = littleEndian ? 1 : 0;
  fs.FirstBlock = firstBlock;
  fs.FirstInum = firstInum;
  fs.LastBlock = lastBlock;
  fs.LastInum = lastInum;
  fs.Flags = flags;
  fs.FsID = fsID;
  fs.JournalInum = journalInum;
  fs.RootInum = rootInum;
  fs.NumInums = numInums;
  fs.RootDirentId = {};
  fs.RootInodeId = {};

  if (State == IMG) {
    fs.VolumeAddr = 0;
    fs.VolumeTableNum = 0;
    State = IMG_FS;
  }
  else if (State == VOL) {
    fs.VolumeAddr = CurVolAddr;
    fs.VolumeTableNum = CurVolTableNum;
    State = VOL_FS;
  }
  else {
    THROW("Attempted to add filesystem in state " << State);
  }

  CurByteOffset = byteOffset;
  ++FsIdx;
  Filesystems.push_back(std::move(fs));
}

void TskImgAssembler::setCurrentRootInodeId(const std::array<uint8_t, 32>& id) {
  THROW_IF(Filesystems.empty(), "setCurrentRootInodeId called before addFileSystem");
  // All-zero array = sentinel for "RootInodeId not yet set" (was std::string::empty() pre-binary-hash migration)
  if (Filesystems.back().RootInodeId == std::array<uint8_t, 32>{}) {
    Filesystems.back().RootInodeId = id;
  }
}
