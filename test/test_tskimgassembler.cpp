#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstdint>
#include <stdexcept>

#include "tskimgassembler.h"
#include "evidencerec.h"

TEST_CASE("assemblerImgVolumeSystemVolumeFS") {
  TskImgAssembler a;

  a.addImage("laptop.E01", "/mnt/evidence/laptop.E01", "ewf", "Expert Witness Format", 500000000000, 512, "");

  a.addVolumeSystem("GPT", "GUID Partition Table", 512);

  a.addVolume(0, 0, "NTFS (0x07)", "Allocated", 1024000, 0, 2048);

  a.addFileSystem(1048576, "ntfs", 4096, 262144, 512, "Cluster", true,
                  0, 0, 262143, 65535, "", "ABCDEF", 0, 5, 65536);

  a.addVolume(1, 0, "Linux (0x83)", "Allocated", 2048000, 1, 1026048);

  auto& evidence = a.evidenceFile();
  REQUIRE(evidence.Name == "laptop.E01");
  REQUIRE(evidence.ImageType == "ewf");
  REQUIRE(evidence.ImageSize == 500000000000);

  auto& volumes = a.volumes();
  REQUIRE(volumes.size() == 2);
  REQUIRE(volumes[0].Addr == 0);
  REQUIRE(volumes[0].VsType == "GPT");
  REQUIRE(volumes[1].Description == "Linux (0x83)");

  auto& filesystems = a.filesystems();
  REQUIRE(filesystems.size() == 1);
  REQUIRE(filesystems[0].ByteOffset == 1048576);
  REQUIRE(filesystems[0].Type == "ntfs");
  REQUIRE(filesystems[0].EvidenceFileName == "laptop.E01");
  REQUIRE(filesystems[0].VolumeAddr == 0);
}

TEST_CASE("assemblerImgFS") {
  TskImgAssembler a;

  a.addImage("raw.dd", "/mnt/raw.dd", "raw", "Single raw file", 1000000, 512, "");
  a.addFileSystem(0, "ext4", 4096, 100000, 512, "Block", false,
                  0, 2, 99999, 10000, "", "1234ABCD", 8, 2, 10001);

  auto& evidence = a.evidenceFile();
  REQUIRE(evidence.Name == "raw.dd");

  auto& filesystems = a.filesystems();
  REQUIRE(filesystems.size() == 1);
  REQUIRE(filesystems[0].ByteOffset == 0);
  REQUIRE(filesystems[0].VolumeAddr == 0);
  REQUIRE(filesystems[0].VolumeTableNum == 0);
}

TEST_CASE("assemblerIllegalTransitionInitToVS") {
  TskImgAssembler a;
  CHECK_THROWS_AS(a.addVolumeSystem("GPT", "", 512), std::runtime_error);
}

TEST_CASE("assemblerIllegalTransitionInitToFS") {
  TskImgAssembler a;
  CHECK_THROWS_AS(a.addFileSystem(0, "ntfs", 4096, 100, 512, "", true, 0, 0, 99, 99, "", "", 0, 5, 100), std::runtime_error);
}

TEST_CASE("assemblerIllegalTransitionImgToVol") {
  TskImgAssembler a;
  a.addImage("test.dd", "/test.dd", "raw", "", 100, 512, "");
  CHECK_THROWS_AS(a.addVolume(0, 0, "", "", 0, 0, 0), std::runtime_error);
}

TEST_CASE("assemblerIllegalTransitionImgFSToFS") {
  TskImgAssembler a;
  a.addImage("test.dd", "/test.dd", "raw", "", 100, 512, "");
  a.addFileSystem(0, "ext4", 4096, 100, 512, "", false, 0, 0, 99, 99, "", "", 0, 2, 100);
  CHECK_THROWS_AS(a.addFileSystem(1024, "ntfs", 4096, 100, 512, "", true, 0, 0, 99, 99, "", "", 0, 5, 100), std::runtime_error);
}

TEST_CASE("assemblerCurrentFsInfo") {
  TskImgAssembler a;
  a.addImage("laptop.E01", "/mnt/laptop.E01", "ewf", "", 500000000000, 512, "");
  a.addVolumeSystem("GPT", "", 512);
  a.addVolume(0, 0, "", "", 0, 0, 0);
  a.addFileSystem(1048576, "ntfs", 4096, 262144, 512, "", true, 0, 0, 262143, 65535, "", "", 0, 5, 65536);

  REQUIRE(a.currentEvidenceFileName() == "laptop.E01");
  REQUIRE(a.currentByteOffset() == 1048576);
  REQUIRE(a.fsIndex() == 1);
}

TEST_CASE("testSetCurrentRootInodeId") {
  TskImgAssembler a;
  a.addImage("disk.E01", "/path/disk.E01", "ewf", "Expert Witness", 1024, 512, "");
  a.addFileSystem(1048576, "ntfs", 4096, 100, 512, "byte",
                  true, 0, 0, 99, 99, "", "abcd1234",
                  0, 5, 100);

  REQUIRE(a.filesystems().size() == 1);
  REQUIRE(a.filesystems().back().RootInodeId == std::array<uint8_t, 32>{});

  std::array<uint8_t, 32> id1{};
  id1[0] = 0xde; id1[1] = 0xad; id1[2] = 0xbe; id1[3] = 0xef;
  a.setCurrentRootInodeId(id1);
  REQUIRE(a.filesystems().back().RootInodeId == id1);

  // Second call must NOT overwrite the first (idempotent on the same FS).
  std::array<uint8_t, 32> id2{};
  id2[0] = 0xca; id2[1] = 0xfe; id2[2] = 0xba; id2[3] = 0xbe;
  a.setCurrentRootInodeId(id2);
  REQUIRE(a.filesystems().back().RootInodeId == id1);
}

TEST_CASE("testSetCurrentRootInodeIdThrowsBeforeAddFileSystem") {
  TskImgAssembler a;
  a.addImage("disk.E01", "/path/disk.E01", "ewf", "Expert Witness", 1024, 512, "");
  std::array<uint8_t, 32> id{};
  id[0] = 0xde; id[1] = 0xad; id[2] = 0xbe; id[3] = 0xef;
  CHECK_THROWS_AS(a.setCurrentRootInodeId(id), std::runtime_error);
}
