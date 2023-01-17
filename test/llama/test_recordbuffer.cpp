#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <iostream>

#include "mockoutputwriter.h"
#include "outputchunk.h"
#include "recordbuffer.h"

TEST_CASE("testRecordBufferFlush") {
  MockOutputWriter mock;
  RecordBuffer r("my-recs", 10u, [&mock](const OutputChunk& c){ mock.OutFiles.push_back(c); });

  r.write("whatever"); // will also write a \n
  REQUIRE(9u == r.size());
  REQUIRE(0u == mock.OutFiles.size());
  r.write("a");
  REQUIRE(11u == r.size());
  REQUIRE(0u == mock.OutFiles.size());
  r.write("b");
  REQUIRE(2u == r.size());
  REQUIRE(1u == mock.OutFiles.size());
  REQUIRE("whatever\na\n" == mock.OutFiles[0].data);
  r.write("this is terrible");
  REQUIRE(1u == mock.OutFiles.size());
  r.flush();
  REQUIRE(0u == r.size());
  REQUIRE(2u == mock.OutFiles.size());
  REQUIRE("b\nthis is terrible\n" == mock.OutFiles[1].data);
}

TEST_CASE("testRecordBufferOutput") {
  MockOutputWriter mock;
  RecordBuffer r("recs/my-recs", 1u, [&mock](const OutputChunk& c){ mock.OutFiles.push_back(c); });
  r.write("a record");
  r.flush();
  REQUIRE(1u == mock.OutFiles.size());
  REQUIRE("recs/my-recs-0001.jsonl" == mock.OutFiles[0].path);
  REQUIRE(9u == mock.OutFiles[0].size);
}

TEST_CASE("testRecordBufferDirectAccess") {
  MockOutputWriter mock;
  RecordBuffer r("your-recs", 10u, [&mock](const OutputChunk& c){ mock.OutFiles.push_back(c); });

  r.get() << "whatever\n";
  REQUIRE(9u == r.size());
  REQUIRE(0u == mock.OutFiles.size());
}

TEST_CASE("testRecordBufferDestructor") {
  MockOutputWriter mock;
  {
    RecordBuffer r("my-recs", 10u, [&mock](const OutputChunk& c){ mock.OutFiles.push_back(c); });
    r.write("whatever");
    REQUIRE(0u == mock.OutFiles.size());;
  }
  REQUIRE(1u == mock.OutFiles.size());;
}
