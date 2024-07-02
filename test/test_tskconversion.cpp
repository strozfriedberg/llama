#include <catch2/catch_test_macros.hpp>

#include "tskconversion.h"

TEST_CASE("textExtractString") {
  using namespace TskUtils;
  REQUIRE("" == extractString("", 0));
  REQUIRE("abcde" == extractString("abcdefg", 5));
  REQUIRE("abc" == extractString("abc\0defg", 5));
}

TEST_CASE("testFlagsString") {
  using namespace TskUtils;

  const static std::array<std::pair<unsigned int, std::string>, 3> fmap{{
    {0b001, "one"},
    {0b010, "two"},
    {0b100, "three"}
  }};

  REQUIRE("" == flagsString(0, fmap));
  REQUIRE("one" == flagsString(1, fmap));
  REQUIRE("two" == flagsString(2, fmap));
  REQUIRE("one, two" == flagsString(3, fmap));
  REQUIRE("three" == flagsString(4, fmap));
  REQUIRE("one, three" == flagsString(5, fmap));
  REQUIRE("two, three" == flagsString(6, fmap));
  REQUIRE("one, two, three" == flagsString(7, fmap));
  REQUIRE("" == flagsString(8, fmap));
}

TEST_CASE("testTskVolumeSystemType") {
  using namespace TskUtils;
  REQUIRE("Unknown" == volumeSystemType(0));
  REQUIRE("MBR" == volumeSystemType(1));
  REQUIRE("BSD" == volumeSystemType(2));
  REQUIRE("Sun" == volumeSystemType(4));
  REQUIRE("Macintosh" == volumeSystemType(8));
  REQUIRE("GPT" == volumeSystemType(16));
  REQUIRE("Unknown" == volumeSystemType(0xffff));
}

TEST_CASE("testTskVolumeFlags") {
  using namespace TskUtils;
  REQUIRE("" == volumeFlags(0));
  REQUIRE("Allocated" == volumeFlags(1));
  REQUIRE("Unallocated" == volumeFlags(2));
  REQUIRE("" == volumeFlags(3));
  REQUIRE("Volume System" == volumeFlags(4));
  REQUIRE("" == volumeFlags(5));
  // I don't think there's much point in testing for that "ALL" enum
}

TEST_CASE("testTskFilesystemFlags") {
  using namespace TskUtils;
  REQUIRE("" == filesystemFlags(0));
  REQUIRE("Sequenced" == filesystemFlags(1));
  REQUIRE("Nanosecond precision" == filesystemFlags(2));
  REQUIRE("Sequenced, Nanosecond precision" == filesystemFlags(3));
}

TEST_CASE("testTskNameType") {
  using namespace TskUtils;
  REQUIRE("Undefined" == nameType(0));
  REQUIRE("Named Pipe" == nameType(1));
  REQUIRE("Character Device" == nameType(2));
  REQUIRE("Folder" == nameType(3));
  REQUIRE("Block Device" == nameType(4));
  REQUIRE("File" == nameType(5));
  REQUIRE("Symbolic Link" == nameType(6));
  REQUIRE("Domain Socket" == nameType(7));
  REQUIRE("Shadow Inode" == nameType(8));
  REQUIRE("Whiteout Inode" == nameType(9));
  REQUIRE("Virtual" == nameType(10));
  REQUIRE("Virtual Folder" == nameType(11));
  REQUIRE("Undefined" == nameType(12));
}

TEST_CASE("testTskNameFlags") {
  using namespace TskUtils;
  REQUIRE("Allocated" == nameFlags(1));
  REQUIRE("Deleted" == nameFlags(2));
}

TEST_CASE("testTskMetaType") {
  // Seriously, same shit as name type, different order
  using namespace TskUtils;
  REQUIRE("Undefined" == metaType(0));
  REQUIRE("File" == metaType(1));
  REQUIRE("Folder" == metaType(2));
  REQUIRE("Named Pipe" == metaType(3));
  REQUIRE("Character Device" == metaType(4));
  REQUIRE("Block Device" == metaType(5));
  REQUIRE("Symbolic Link" == metaType(6));
  REQUIRE("Shadow Inode" == metaType(7));
  REQUIRE("Domain Socket" == metaType(8));
  REQUIRE("Whiteout Inode" == metaType(9));
  REQUIRE("Virtual" == metaType(10));
  REQUIRE("Virtual Folder" == metaType(11));
  REQUIRE("Undefined" == metaType(12));
}

TEST_CASE("testTskMetaFlags") {
  using namespace TskUtils;
  REQUIRE("Allocated" == metaFlags(1));
  REQUIRE("Deleted" == metaFlags(2));
  REQUIRE("Used" == metaFlags(4));
  REQUIRE("Unused" == metaFlags(8));
  REQUIRE("Compressed" == metaFlags(16));
  REQUIRE("Orphan" == metaFlags(32));
  REQUIRE("Deleted, Used" == metaFlags(6));
}

TEST_CASE("testTskAttrType") {
  using namespace TskUtils;
  REQUIRE("Unknown" == attrType(TSK_FS_ATTR_TYPE_NOT_FOUND));
  REQUIRE("Data" == attrType(TSK_FS_ATTR_TYPE_DEFAULT)); // default _is_ data, so match up with NTFS Data
  REQUIRE("Standard Information" == attrType(TSK_FS_ATTR_TYPE_NTFS_SI));
  REQUIRE("NTFS Attributes" == attrType(TSK_FS_ATTR_TYPE_NTFS_ATTRLIST));
  REQUIRE("Filename" == attrType(TSK_FS_ATTR_TYPE_NTFS_FNAME));
  REQUIRE("ObjID" == attrType(TSK_FS_ATTR_TYPE_NTFS_OBJID));
  REQUIRE("Sec" == attrType(TSK_FS_ATTR_TYPE_NTFS_SEC));
  REQUIRE("VName" == attrType(TSK_FS_ATTR_TYPE_NTFS_VNAME));
  REQUIRE("VInfo" == attrType(TSK_FS_ATTR_TYPE_NTFS_VINFO));
  REQUIRE("Data" == attrType(TSK_FS_ATTR_TYPE_NTFS_DATA));
  REQUIRE("IdxRoot" == attrType(TSK_FS_ATTR_TYPE_NTFS_IDXROOT));
  REQUIRE("IdxAlloc" == attrType(TSK_FS_ATTR_TYPE_NTFS_IDXALLOC));
  REQUIRE("Bitmap" == attrType(TSK_FS_ATTR_TYPE_NTFS_BITMAP));
  REQUIRE("Symlink" == attrType(TSK_FS_ATTR_TYPE_NTFS_SYMLNK));

  // It would be great if this would return Reparse, but it can't because
  // reparse has the same typecode as symlink. So instead we assert the
  // broken behavior. This was a change from Win2K and WinXP.
  REQUIRE("Symlink" == attrType(TSK_FS_ATTR_TYPE_NTFS_REPARSE));

  REQUIRE("EAInfo" == attrType(TSK_FS_ATTR_TYPE_NTFS_EAINFO));
  REQUIRE("EA" == attrType(TSK_FS_ATTR_TYPE_NTFS_EA));
  REQUIRE("Prop" == attrType(TSK_FS_ATTR_TYPE_NTFS_PROP));
  REQUIRE("Log" == attrType(TSK_FS_ATTR_TYPE_NTFS_LOG));

  REQUIRE("Indirect" == attrType(TSK_FS_ATTR_TYPE_UNIX_INDIR));
  REQUIRE("Extents" == attrType(TSK_FS_ATTR_TYPE_UNIX_EXTENT));

  // Types for HFS+ File Attributes
  REQUIRE("Data" == attrType(TSK_FS_ATTR_TYPE_HFS_DEFAULT));
  REQUIRE("Data" == attrType(TSK_FS_ATTR_TYPE_HFS_DATA));
  REQUIRE("Resource" == attrType(TSK_FS_ATTR_TYPE_HFS_RSRC));
  REQUIRE("EA" == attrType(TSK_FS_ATTR_TYPE_HFS_EXT_ATTR));
  REQUIRE("Compressed" == attrType(TSK_FS_ATTR_TYPE_HFS_COMP_REC));
}

TEST_CASE("testTskAttrFlags") {
  using namespace TskUtils;
  REQUIRE("" == attrFlags(0));
  REQUIRE("In Use" == attrFlags(1));
  REQUIRE("Non-resident" == attrFlags(2));
  REQUIRE("Resident" == attrFlags(4));
  REQUIRE("Encrypted" == attrFlags(16));
  REQUIRE("Compressed" == attrFlags(32));
  REQUIRE("Sparse" == attrFlags(64));
  REQUIRE("Recovered" == attrFlags(128));
  REQUIRE("In Use, Non-resident" == attrFlags(3));
}

TEST_CASE("testTskNrdRunFlags") {
  using namespace TskUtils;
  REQUIRE("" == nrdRunFlags(TSK_FS_ATTR_RUN_FLAG_NONE));
  REQUIRE("Filler" == nrdRunFlags(TSK_FS_ATTR_RUN_FLAG_FILLER));
  REQUIRE("Sparse" == nrdRunFlags(TSK_FS_ATTR_RUN_FLAG_SPARSE));
}

TEST_CASE("testTskFileSystemID") {
  using namespace TskUtils;
  const uint8_t id[] = { 0xDE, 0xAD, 0xBE, 0xEF };
  REQUIRE("deadbeef" == filesystemID(id, sizeof(id), false));
  REQUIRE("efbeadde" == filesystemID(id, sizeof(id), true));
}

TEST_CASE("testTskConvertFS") {
  TSK_FS_INFO fs;
  std::memset(&fs, 0, sizeof(fs));

  fs.offset = 1;
  fs.inum_count = 2;
  fs.root_inum = 3;
  fs.first_inum = 4;
  fs.last_inum = 5;
  fs.block_count = 6;
  fs.first_block = 7;
  fs.last_block = 8;
  fs.block_size = 9;
  fs.dev_bsize = 10;
  fs.block_pre_size = 11;
  fs.block_post_size = 12;
  fs.journ_inum = 13;
  fs.ftype = TSK_FS_TYPE_FAT16;
  fs.duname = "whatever";
  fs.flags = static_cast<TSK_FS_INFO_FLAG_ENUM>(TSK_FS_INFO_FLAG_HAVE_SEQ | TSK_FS_INFO_FLAG_HAVE_NANOSEC);
  REQUIRE(8 <= TSK_FS_INFO_FS_ID_LEN);
  fs.fs_id[0] = 0x01;
  fs.fs_id[1] = 0x23;
  fs.fs_id[2] = 0x45;
  fs.fs_id[3] = 0x67;
  fs.fs_id[4] = 0x89;
  fs.fs_id[5] = 0xAB;
  fs.fs_id[6] = 0xCD;
  fs.fs_id[7] = 0xEF;
  fs.fs_id_used = 8;
  fs.endian = TSK_BIG_ENDIAN;

  const jsoncons::json js = TskUtils::convertFS(fs);
  const std::string expected = "{\"blockName\":\"whatever\",\"blockSize\":9,\"byteOffset\":1,\"deviceBlockSize\":10,\"firstBlock\":7,\"firstInum\":4,\"flags\":\"Sequenced, Nanosecond precision\",\"fsID\":\"0123456789abcdef\",\"journalInum\":13,\"lastBlock\":8,\"lastBlockAct\":0,\"lastInum\":5,\"littleEndian\":false,\"numBlocks\":6,\"numInums\":2,\"rootInum\":3,\"type\":\"fat16\"}";
  const std::string actual = js.as<std::string>();
  REQUIRE(expected == actual);
}

TEST_CASE("testTskConvertVol") {
  TSK_VS_PART_INFO vol;
  std::memset(&vol, 0, sizeof(vol));

  vol.start = 1;
  vol.len = 11; // this volume goes to 11
  vol.desc = const_cast<char*>("TURN IT UP");
  vol.table_num = 2;
  vol.slot_num = 3;
  vol.addr = 4;
  vol.flags = TSK_VS_PART_FLAG_META;

  const jsoncons::json js = TskUtils::convertVol(vol);
  const std::string expected = "{\"addr\":4,\"description\":\"TURN IT UP\",\"flags\":\"Volume System\",\"numBlocks\":11,\"slotNum\":3,\"startBlock\":1,\"tableNum\":2}";
  const std::string actual = js.as<std::string>();
  REQUIRE(expected == actual);
}

TEST_CASE("testTskConvertVS") {
  TSK_VS_INFO vs;
  std::memset(&vs, 0, sizeof(vs));

  vs.vstype = TSK_VS_TYPE_BSD;
  vs.is_backup = 1;
  vs.offset = 2;
  vs.block_size = 3;
  vs.endian = TSK_BIG_ENDIAN;
  vs.part_count = 4;

  const jsoncons::json js = TskUtils::convertVS(vs);
  const std::string expected = "{\"blockSize\":3,\"description\":\"BSD Disk Label\",\"numVolumes\":4,\"offset\":2,\"type\":\"BSD\",\"volumes\":[]}";
  const std::string actual = js.as<std::string>();
  REQUIRE(expected == actual);
}

TEST_CASE("testTskConvertImg") {
  TSK_IMG_INFO img;
  std::memset(&img, 0, sizeof(img));

  img.itype = TSK_IMG_TYPE_EWF_EWF;
  img.size = 1;
  img.num_img = 2;
  img.sector_size = 3;
  img.page_size = 4;
  img.spare_size = 5;

  const jsoncons::json js = TskUtils::convertImg(img);
  const std::string expected = "{\"description\":\"Expert Witness Format (EnCase)\",\"sectorSize\":3,\"size\":1,\"type\":\"ewf\"}";
  const std::string actual = js.as<std::string>();
  REQUIRE(expected == actual);
}

TEST_CASE("testTskConvertRun") {
  TSK_FS_ATTR_RUN nrd;
  std::memset(&nrd, 0, sizeof(nrd));

  nrd.addr   = 15;
  nrd.len    = 3045;
  nrd.offset = 17;
  nrd.flags  = TSK_FS_ATTR_RUN_FLAG_SPARSE;

  const jsoncons::json js = TskUtils::convertRun(nrd);
  const std::string expected = "{\"addr\":15,\"flags\":\"Sparse\",\"len\":3045,\"offset\":17}";
  const std::string actual = js.as<std::string>();
  REQUIRE(expected == actual);
}

TSK_FS_ATTR* setResAttr(TSK_FS_ATTR& attr) {
  attr.flags = static_cast<TSK_FS_ATTR_FLAG_ENUM>(TSK_FS_ATTR_RES | TSK_FS_ATTR_INUSE);
  attr.id = 1;
  attr.name = const_cast<char*>("$DATA\0 3.1459"); // will be clipped at null character
  attr.name_size = 13;
  attr.next = nullptr;
  attr.rd.buf = (unsigned char*)("whatever\tslack");
  attr.rd.buf_size = 14;
  attr.rd.offset = 0;
  attr.size = 8;
  attr.type = TSK_FS_ATTR_TYPE_NTFS_DATA;
  return &attr;
}

TEST_CASE("testTskConvertAttrRes") {
  TSK_FS_ATTR attr;
  std::memset(&attr, 0, sizeof(attr));
  setResAttr(attr);

  const jsoncons::json js = TskUtils::convertAttr(attr);
  REQUIRE(1 == js["id"]);
  REQUIRE("In Use, Resident" == js["flags"]);
  REQUIRE("$DATA" == js["name"]);
  REQUIRE("7768617465766572" == js["rd_buf"]);
  REQUIRE(0 == js["rd_offset"]);
  REQUIRE(8u == js["size"]);
  REQUIRE("Data" == js["type"]);
}

TSK_FS_ATTR* setNonresAttr(TSK_FS_ATTR& attr, TSK_FS_ATTR_RUN& nrd1, TSK_FS_ATTR_RUN& nrd2) {
  nrd1.addr = 12;
  nrd1.flags = TSK_FS_ATTR_RUN_FLAG_NONE;
  nrd1.len = 2;
  nrd1.offset = 0;
  nrd1.next = &nrd2;

  nrd2.addr = 38;
  nrd2.flags = TSK_FS_ATTR_RUN_FLAG_NONE;
  nrd2.len = 1;
  nrd2.offset = 2;
  nrd2.next = nullptr;

  attr.flags = (TSK_FS_ATTR_FLAG_ENUM)(TSK_FS_ATTR_NONRES | TSK_FS_ATTR_INUSE);
  attr.id = 4;
  attr.name = const_cast<char*>("$DATA");
  attr.name_size = 5;
  attr.next = nullptr;
  // Set up the nrds
  attr.nrd.run = &nrd1;
  attr.nrd.run_end = &nrd2;
  attr.nrd.allocsize = 1536;
  // The following aren't values we'd expect to see, but test serialization well enough
  attr.nrd.compsize = 3;
  attr.nrd.initsize = 6;
  attr.nrd.skiplen = 9;
  // These should be ignored due to flags
  attr.rd.buf = (unsigned char*)("whatever");
  attr.rd.buf_size = 8;
  attr.rd.offset = 5;

  attr.size = 1235;
  attr.type = TSK_FS_ATTR_TYPE_NTFS_DATA;
  return &attr;
}

void testNonresAttr(const jsoncons::json& js) {
  REQUIRE(4 == js["id"]);
  REQUIRE("In Use, Non-resident" == js["flags"]);
  REQUIRE("$DATA" == js["name"]);

  REQUIRE(!js.contains("rd_buf"));
  REQUIRE(!js.contains("rd_offset"));
  REQUIRE(2u == js["nrd_runs"].size());
  REQUIRE(12 == js["nrd_runs"][0]["addr"]);
  REQUIRE(38 == js["nrd_runs"][1]["addr"]);
  REQUIRE(1536 == js["nrd_allocsize"]);
  REQUIRE(3 == js["nrd_compsize"]);
  REQUIRE(6 == js["nrd_initsize"]);
  REQUIRE(9 == js["nrd_skiplen"]);

  REQUIRE(1235u == js["size"]);
  REQUIRE("Data" == js["type"]);
}

TEST_CASE("testTskConvertAttrNonRes") {
  TSK_FS_ATTR_RUN nrd1;
  std::memset(&nrd1, 0, sizeof(nrd1));

  TSK_FS_ATTR_RUN nrd2;
  std::memset(&nrd2, 0, sizeof(nrd2));

  TSK_FS_ATTR attr;
  std::memset(&attr, 0, sizeof(attr));
  setNonresAttr(attr, nrd1, nrd2);

  jsoncons::json js = TskUtils::convertAttr(attr);
  js["nrd_runs"].push_back(TskUtils::convertRun(nrd1));
  js["nrd_runs"].push_back(TskUtils::convertRun(nrd2));
  testNonresAttr(js);
}

TEST_CASE("testTskMetaConvert") {
  TSK_FS_META meta;
  std::memset(&meta, 0, sizeof(meta));

  meta.addr = 17;
  meta.flags = TSK_FS_META_FLAG_UNALLOC;
  meta.type = TSK_FS_META_TYPE_REG;

  meta.uid = 21;
  meta.gid = 1026;

  meta.link = const_cast<char*>("I_am_the_target"); // terrible that there's no link_size
  meta.nlink = 2;
  meta.seq = 8;

  meta.atime = 1578364822; // 2020-01-07 02:40:22
  meta.atime_nano = 123456700;
  meta.crtime = 31337;     // 1970-01-01 08:42:17
  meta.crtime_nano = 123456400;
  meta.ctime = 234123870;  // 1977-06-02 18:24:30
  meta.ctime_nano = 315227845;
  meta.mtime = 314159265;  // 1979-12-16 02:27:45
  meta.mtime_nano = 999999999;

  // meta.name2 = "SHRTNM~2";

  CommonTimestampGetter tsg;
  jsoncons::json js = TskUtils::convertMeta(meta, tsg);

  REQUIRE(17 == js["addr"]);
  REQUIRE("Deleted" == js["flags"]);
  REQUIRE("File" == js["type"]);

  REQUIRE("21" == js["uid"]); // stringized
  REQUIRE("1026" == js["gid"]); // also stringized

  REQUIRE("I_am_the_target" == js["link"]);
  REQUIRE(2 == js["nlink"]);
  REQUIRE(8 == js["seq"]);
  // REQUIRE("SHRTNM~2" == js["name2"]);

  // basic four are good
  REQUIRE("2020-01-07 02:40:22.1234567" == js.at("accessed"));
  REQUIRE("1970-01-01 08:42:17.1234564" == js.at("created"));
  REQUIRE("1977-06-02 18:24:30.315227845" == js.at("metadata"));
  REQUIRE("1979-12-16 02:27:45.999999999" == js.at("modified"));
  // others not so much
  REQUIRE(js.at("deleted").is_null());
  REQUIRE(js.at("backup").is_null());
  REQUIRE(js.at("fn_accessed").is_null());
  REQUIRE(js.at("fn_created").is_null());
  REQUIRE(js.at("fn_metadata").is_null());
  REQUIRE(js.at("fn_modified").is_null());

  meta.link = nullptr;
  js = TskUtils::convertMeta(meta, tsg);
  REQUIRE("" == js["link"]);
}

namespace {
  void initTskFsName(TSK_FS_NAME& name) {
    std::memset(&name, 0, sizeof(name));

    name.name = const_cast<char*>("woowoowoo\0bad bad bad");
    name.name_size = 9;
    name.shrt_name = const_cast<char*>("WOOWOO~1");
    name.shrt_name_size = 8;
    name.meta_addr = 7;
    name.meta_seq = 6;
    name.par_addr = 231;
    name.par_seq = 72;
    name.type = TSK_FS_NAME_TYPE_SOCK;
    name.flags = TSK_FS_NAME_FLAG_ALLOC;
  }
}

TEST_CASE("testTskNameConvert") {
  TSK_FS_NAME name;
  initTskFsName(name);

  const jsoncons::json js = TskUtils::convertName(name);

  REQUIRE("woowoowoo" == js["name"]);
  REQUIRE("WOOWOO~1" == js["shrt_name"]);
  REQUIRE(7 == js["meta_addr"]);
  REQUIRE(6 == js["meta_seq"]);
  REQUIRE(231 == js["par_addr"]);
  REQUIRE(72 == js["par_seq"]);
  REQUIRE("Domain Socket" == js["type"]);
  REQUIRE("Allocated" == js["flags"]);
}
