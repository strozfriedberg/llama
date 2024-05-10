/***********************
 * 
 * 
 * There's some code at the bottom that was already commented out, presumably because it was 
 * in progress or being contemplated.
 * 
 * The first two test cases were commented out because how they test is via a whole lot of
 * mocking of TSK, yet what they're testing is not much. Essentially, the two tests document
 * the difference between overall Image-related json between a disk image that has a partition
 * table (i.e., VolumeSystem) and one that has just a raw volume on it (typically seen with USB 
 * thumb drives). This is an important, and annoying, difference to test and document, so this 
 * test code does not need to be revisited.
 * 
 * However, the tests proceed by creating mocks of mocks of mocks (e.g., DummyTskBase -> 
 * FakeTskBase -> FakeTskWithVolumeSystem). There's then quite a laborious setup of a several 
 * stub overrides and a bunch of jsoncons objects. With that... there doesn't seem like there's
 * much actually being tested: conversion of some TSK structs to json? But there are other places for that.
 * The attendant tsk function-driven iteration necessary for walking volumes in a partition table?
 * 
 * Plugging into InputReader also seems like a mistake, moving us from the realm of unit tests 
 * and into integration tests... but we don't need to perform integration tests with so many mock
 * components.
 * 
 * TODO: Revisit once some things have been refactored with TSK usage.
 * 
 *
*/

// #include <catch2/catch_test_macros.hpp>

// #include <cstring>
// #include <stdexcept>

// #include "tskreader.h"

// #include "dummytsk.h"
// #include "mockinputhandler.h"
// #include "mockoutputhandler.h"

// void noop_deleter(TSK_IMG_INFO*) {}

// class FakeTskBase: public DummyTsk {
// public:
//   FakeTskBase() {
//     std::memset(&Img, 0, sizeof(Img));
//   }

//   std::unique_ptr<TSK_IMG_INFO, void(*)(TSK_IMG_INFO*)> openImg(const char*) const override {
//     return {&Img, noop_deleter};
//   }

//   jsoncons::json convertImg(const TSK_IMG_INFO&) const override {
//     return jsoncons::json(jsoncons::json_object_arg);
//   }

//   jsoncons::json convertVS(const TSK_VS_INFO&) const override {
//     return jsoncons::json(
//       jsoncons::json_object_arg,
//       {
//         { "volumes", jsoncons::json(jsoncons::json_array_arg) }
//       }
//     );
//   }

//   jsoncons::json convertVol(const TSK_VS_PART_INFO&) const override {
//     return jsoncons::json(jsoncons::json_object_arg);
//   }

//   jsoncons::json convertFS(const TSK_FS_INFO&) const override {
//     return jsoncons::json(jsoncons::json_object_arg);
//   }

//   jsoncons::json convertName(const TSK_FS_NAME&) const override {
//     return jsoncons::json(jsoncons::json_object_arg);
//   }

//   jsoncons::json convertMeta(const TSK_FS_META&, TimestampGetter&) const override {
//     return jsoncons::json(
//       jsoncons::json_object_arg,
//       {
//         { "attrs", jsoncons::json(jsoncons::json_array_arg) }
//       }
//     );
//   }

//   jsoncons::json convertAttr(const TSK_FS_ATTR&) const override {
//     return jsoncons::json(jsoncons::json_object_arg);
//   }

// protected:
//   mutable TSK_IMG_INFO Img; // because openImg() is const yet takes fake ownership of a member
// };

// class FakeTskWithVolumeSystem: public FakeTskBase {
// public:
//   bool walk(
//     TSK_IMG_INFO* /*info*/,
//     std::function<TSK_FILTER_ENUM(const TSK_VS_INFO*)> vs_cb,
//     std::function<TSK_FILTER_ENUM(const TSK_VS_PART_INFO*)> vol_cb,
//     std::function<TSK_FILTER_ENUM(TSK_FS_INFO*)> fs_cb,
//     std::function<TSK_RETVAL_ENUM(TSK_FS_FILE*, const char*)>
//   ) override
//   {
//     TSK_VS_INFO vs;
//     std::memset(&vs, 0, sizeof(vs));
//     vs_cb(&vs);

//     TSK_VS_PART_INFO vol;
//     std::memset(&vol, 0, sizeof(vol));

//     TSK_FS_INFO fs;
//     std::memset(&fs, 0, sizeof(fs));

//     // first volume
//     vol_cb(&vol);
//     fs_cb(&fs);

//     // second volume -- no filesystem!
//     vol_cb(&vol);

//     // third volume
//     vol_cb(&vol);
//     fs_cb(&fs);

//     return true;
//   }
// };

// TEST_CASE("testTskReaderVolumeSystem") {
//   TskReader<FakeTskWithVolumeSystem> r("bogus.E01");

//   auto ih = std::shared_ptr<MockInputHandler>(new MockInputHandler());
//   r.setInputHandler(std::static_pointer_cast<InputHandler>(ih));

//   auto oh = std::shared_ptr<MockOutputHandler>(new MockOutputHandler());
//   r.setOutputHandler(std::static_pointer_cast<OutputHandler>(oh));

//   REQUIRE(r.open());
//   REQUIRE(r.startReading());

//   REQUIRE(1u == oh->Images.size());

//   const jsoncons::json exp(
//     jsoncons::json_object_arg,
//     {
//       {
//         "volumeSystem",
//         jsoncons::json(
//           jsoncons::json_object_arg,
//           {
//             {
//               "volumes",
//               jsoncons::json(
//                 // volume 1
//                 jsoncons::json_array_arg,
//                 {
//                   jsoncons::json(
//                     jsoncons::json_object_arg,
//                     {
//                       {
//                         "fileSystem",
//                         jsoncons::json(jsoncons::json_object_arg)
//                       }
//                     }
//                   ),
//                   // volume 2
//                   jsoncons::json(jsoncons::json_object_arg),
//                   // volume 3
//                   jsoncons::json(
//                     jsoncons::json_object_arg,
//                     {
//                       {
//                         "fileSystem",
//                         jsoncons::json(jsoncons::json_object_arg)
//                       }
//                     }
//                   )
//                 }
//               )
//             }
//           }
//         )
//       }
//     }
//   );

//   REQUIRE(exp == oh->Images[0].Doc);

//   REQUIRE(oh->Dirents.empty());
//   REQUIRE(oh->Inodes.empty());
//   REQUIRE(ih->Batch.empty());
// }

// class FakeTskWithNoVolumeSystem: public FakeTskBase {
// public:
//   bool walk(
//     TSK_IMG_INFO* /*info*/,
//     std::function<TSK_FILTER_ENUM(const TSK_VS_INFO*)>,
//     std::function<TSK_FILTER_ENUM(const TSK_VS_PART_INFO*)>,
//     std::function<TSK_FILTER_ENUM(TSK_FS_INFO*)> fs_cb,
//     std::function<TSK_RETVAL_ENUM(TSK_FS_FILE*, const char*)>
//   )
//   {
//     TSK_FS_INFO fs;
//     std::memset(&fs, 0, sizeof(fs));
//     fs_cb(&fs);

//     return true;
//   }
// };

// TEST_CASE("testTskReaderNoVolumeSystem") {
//   TskReader<FakeTskWithNoVolumeSystem> r("bogus.E01");

//   auto ih = std::shared_ptr<MockInputHandler>(new MockInputHandler());
//   r.setInputHandler(std::static_pointer_cast<InputHandler>(ih));

//   auto oh = std::shared_ptr<MockOutputHandler>(new MockOutputHandler());
//   r.setOutputHandler(std::static_pointer_cast<OutputHandler>(oh));

//   REQUIRE(r.open());
//   REQUIRE(r.startReading());

//   REQUIRE(1u == oh->Images.size());

//   const jsoncons::json exp(
//     jsoncons::json_object_arg,
//     {
//       {
//         "fileSystem",
//         jsoncons::json(jsoncons::json_object_arg)
//       }
//     }
//   );

//   REQUIRE(exp == oh->Images[0].Doc);

//   REQUIRE(oh->Dirents.empty());
//   REQUIRE(oh->Inodes.empty());
//   REQUIRE(ih->Batch.empty());
// }

/*
#include <cstring>


#include "filerecord.h"
#include "mockinputhandler.h"

TEST_CASE("testInodeDedupe") {
  TskReader reader("not_an_image.E01");

  reader.setInodeRange(0, 20);

  TSK_FS_ATTR attrRes;
  std::memset(&attrRes, 0, sizeof(attrRes));
  attrRes.flags = TSK_FS_ATTR_RES;
  attrRes.id = 1;
  attrRes.name = const_cast<char*>("$DATA");
  attrRes.name_size = 5;
  attrRes.next = nullptr;
  attrRes.rd.buf = (unsigned char*)("whatever");
  attrRes.rd.buf_size = 8;
  attrRes.rd.offset = 0;
  attrRes.size = 9;
  attrRes.type = TSK_FS_ATTR_TYPE_NTFS_DATA;

  TSK_FS_ATTRLIST alist;
  std::memset(&alist, 0, sizeof(alist));
  alist.head = &attrRes;

  TSK_FS_INFO fsInfo;
  std::memset(&fsInfo, 0, sizeof(fsInfo));
  fsInfo.ftype = TSK_FS_TYPE_NTFS;

  TSK_FS_META meta;
  std::memset(&meta, 0, sizeof(meta));
  meta.attr = &alist;
  meta.attr_state = TSK_FS_META_ATTR_STUDIED; // make TSK happy
  meta.link = const_cast<char*>("");

  TSK_FS_FILE myFile;
  std::memset(&myFile, 0, sizeof(myFile));
  myFile.meta = &meta;
  myFile.fs_info = &fsInfo;

  auto in = std::shared_ptr<MockInputHandler>(new MockInputHandler());
  reader.setInputHandler(std::static_pointer_cast<InputHandler>(in));

  meta.addr = 8;
  REQUIRE(reader.addToBatch(&myFile));
  REQUIRE(1u == in->batch.size());
  REQUIRE(8u == in->batch.back().Doc["addr"]);

  meta.addr = 9;
  REQUIRE(reader.addToBatch(&myFile));
  REQUIRE(2u == in->batch.size());
  REQUIRE(9u == in->batch.back().Doc["addr"]);

  meta.addr = 8; // dupe!
  REQUIRE(!reader.addToBatch(&myFile));
  REQUIRE(2u == in->batch.size());
}
*/
