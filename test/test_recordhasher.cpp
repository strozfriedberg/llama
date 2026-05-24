#include <catch2/catch_test_macros.hpp>

#include "direntbatch.h"
#include "hex.h"
#include "inode.h"
#include "recordhasher.h"

TEST_CASE("testHashRun") {
  RecordHasher hasher;

  const jsoncons::json r(
    jsoncons::json_object_arg,
    {
      { "offset", 1 },
      { "addr", 2 },
      { "len", 3 },
      { "flags", "Checkered" }
    }
  );

  const FieldHash exp{{
    0x05, 0x50, 0x2d, 0x97, 0x3c, 0x6b, 0x58, 0x6c,
    0x7d, 0x14, 0x88, 0xc2, 0x2f, 0x87, 0x7b, 0x68,
    0x18, 0xe3, 0xc9, 0xc0, 0xd0, 0xf5, 0x6a, 0xd2,
    0xe7, 0x41, 0x76, 0x44, 0x83, 0x2c, 0xfc, 0x0d
  }};

  REQUIRE(exp == hasher.hashRun(r));
}

TEST_CASE("testHashStream") {
  RecordHasher hasher;

  const jsoncons::json r(
    jsoncons::json_object_arg,
    {
      { "id", 1 },
      { "type", 2 },
      { "size", 3 },
      { "name", "Bob" },
      { "flags", "Red" },
      { "data_id", "!#$%^" }
    }
  );

  const FieldHash exp{{
    0x26, 0x8f, 0xc5, 0x74, 0xd9, 0xc1, 0x79, 0x2e,
    0xed, 0x35, 0x22, 0x99, 0x0c, 0x0b, 0x03, 0xe2,
    0x69, 0x50, 0xb2, 0x75, 0x14, 0xc7, 0x1f, 0x94,
    0xfc, 0xb5, 0xd0, 0x7b, 0x50, 0x2a, 0x5b, 0xce
  }};

  REQUIRE(exp == hasher.hashStream(r));
}

TEST_CASE("testHashAttr") {
  RecordHasher hasher;

  const jsoncons::json sr(
    jsoncons::json_object_arg,
    {
      { "id", 1 },
      { "type", 2 },
      { "size", 3 },
      { "name", "Bob" },
      { "flags", "Red" },
      { "data_id", "!#$%^" }
    }
  );

  const jsoncons::json nrds(
    jsoncons::json_array_arg,
    {
      jsoncons::json(
        jsoncons::json_object_arg,
        {
          { "offset", 1 },
          { "addr", 2 },
          { "len", 3 },
          { "flags", "abc" }
        }
      ),
      jsoncons::json(
        jsoncons::json_object_arg,
        {
          { "offset", 4 },
          { "addr", 5 },
          { "len", 6 },
          { "flags", "def" }
        }
      ),
      jsoncons::json(
        jsoncons::json_object_arg,
        {
          { "offset", 7 },
          { "addr", 8 },
          { "len", 9 },
          { "flags", "ghi" }
        }
      )
    }
  );

  const uint8_t buf[20] = { 0xFE };

  const jsoncons::json r = jsoncons::json(
    jsoncons::json_object_arg,
    {
      { "stream", sr },
      { "init_size", 1 },
      { "comp_size",  2 },
      { "rd_buf_size", 20 },
      { "rd_buf", hexEncode(buf, sizeof(buf)) },
      { "skip_len", 5 },
      { "alloc_size", 6 },
      { "nrd_allocsize", 7 },
      { "nrd_skiplen", 8 },
      { "nrd_compsize", 9 },
      { "nrd_initsize", 10 },
      { "nrd_runs", nrds }
    }
  );

  const FieldHash exp{{
    0xb3, 0x37, 0xfd, 0xcc, 0xb3, 0xb4, 0x2e, 0xb2,
    0x53, 0x23, 0xa2, 0xc4, 0xe6, 0xf2, 0x43, 0xb2,
    0xb2, 0x4f, 0x17, 0x57, 0xf0, 0x66, 0x54, 0x06,
    0x04, 0x5b, 0x4c, 0x0f, 0x69, 0xb8, 0xde, 0xbf
  }};

  REQUIRE(exp == hasher.hashAttr(r));
}

TEST_CASE("testHashInode") {
  RecordHasher hasher;

  const std::string in = R"(
{
  "addr": 562,
  "flags": "Allocated, Used",
  "link": "",
  "nlink": 1,
  "seq": 0,
  "type": "File",
  "uid": "0",
  "gid": "0",
  "attrs": [
    {
      "stream": {
        "id": 0,
        "flags": "In Use,Non-resident",
        "name": "",
        "size": 1404014,
        "type": 1,
        "data_id": ""
      },
      "alloc_size": 1409024,
      "comp_size": 0,
      "init_size": 1404014,
      "skip_len": 0,
      "rd_buf": "",
      "rd_buf_size": 0,
      "nrd_allocsize": 1024,
      "nrd_initsize": 1024,
      "nrd_skiplen": 5,
      "nrd_compsize": 5,
      "nrd_runs": [
        {
          "addr": 35984,
          "flags": "",
          "len": 2752,
          "offset": 0
        }
      ]
    }
  ],
  "timestamps": {
    "created": "2009-04-30 15:35:16.3",
    "modified": "2009-04-29 09:00:24",
    "accessed": "2015-02-09 00:00:00"
  }
})";

  const jsoncons::json r = jsoncons::json::parse(in);

  const FieldHash exp{{
    0xb0, 0xbb, 0x2a, 0x3c, 0x8f, 0x6f, 0xbb, 0xcf,
    0x99, 0x98, 0x88, 0x7e, 0xdd, 0x6a, 0x75, 0x5c,
    0x64, 0xcc, 0xef, 0x6a, 0xf0, 0xb4, 0x91, 0xaa,
    0xd6, 0xe6, 0xee, 0xf8, 0xd2, 0x6b, 0x59, 0x49
  }};

  REQUIRE(exp == hasher.hashInode(r));
}

TEST_CASE("testHashDirent") {
  RecordHasher hasher;

  const std::string in = R"(
{
  "type": "File",
  "flags": "",
  "path": "/foo/bar" ,
  "streams": [
    "33328c75c5a0856fe73a66d0a7501dd7c860bbbbb9c334854945eef23d39cf9c",
    "8f3a29581a930c5f89698e819e40c8edf08391d5bb0ca8664b62291cbe30686f"
  ],
  "children": [
    "3c3b55265bc4ebb040da923cbdcd81ee0ab1250e0d1775d5e57caed9bf272b48",
    "04ad441d5836a5ddff1f7da179e9be462f58b5ad9647508715af3283b4e42611"
  ]
}
)";

  const jsoncons::json r = jsoncons::json::parse(in);

  const FieldHash exp{{
    0x6b, 0xa9, 0xe0, 0x32, 0xc1, 0xe1, 0xa4, 0x21,
    0xac, 0x80, 0x63, 0x5e, 0xe1, 0xac, 0x44, 0xbc,
    0x26, 0xbc, 0xf8, 0xf9, 0x4d, 0x2e, 0x17, 0xf8,
    0xb5, 0x06, 0x89, 0x33, 0x52, 0x19, 0xdf, 0x4d
  }};

  REQUIRE(exp == hasher.hashDirent(r));
}

TEST_CASE("testHashDirentClass") {
  Dirent d1{
    "",
    "/foo/bar",
    "baz",
    "b",
    "File",
    "",
    0x12345678,
    0x87654321,
    0x87654321,
    0x12345678
  };
  Dirent d2{
    "",
    "/foo/bar",
    "baz",
    "b",
    "File",
    "Deleted",
    0x12345678,
    0x87654321,
    0x87654321,
    0x12345678
  };

  RecordHasher hasher;
  REQUIRE(hasher.hashDirent(d1) != hasher.hashDirent(d2));
}

TEST_CASE("testHashInodeIdentity") {
  RecordHasher hasher;
  // Inputs: ("disk.E01", 1048576, 5, 5)
  const FieldHash got = hasher.hashInodeIdentity("disk.E01", 1048576, 5, 5);
  // Round-trip: hashing the same inputs again must produce the same bytes.
  const FieldHash again = hasher.hashInodeIdentity("disk.E01", 1048576, 5, 5);
  REQUIRE(got == again);
  // And a different SeqNum must produce a different hash (proves SeqNum is hashed).
  const FieldHash diffSeq = hasher.hashInodeIdentity("disk.E01", 1048576, 5, 6);
  REQUIRE(got != diffSeq);
  // Different EvidenceFileName must also differ.
  const FieldHash diffEfn = hasher.hashInodeIdentity("other.E01", 1048576, 5, 5);
  REQUIRE(got != diffEfn);
}

TEST_CASE("testHashInodeStruct") {
  RecordHasher hasher;
  Inode inode{};
  inode.EvidenceFileName = "disk.E01";
  inode.ByteOffset = 1048576;
  inode.Addr = 5;
  inode.SeqNum = 5;
  const FieldHash viaStruct = hasher.hashInode(inode);
  const FieldHash viaHelper = hasher.hashInodeIdentity("disk.E01", 1048576, 5, 5);
  REQUIRE(viaStruct == viaHelper);
}

