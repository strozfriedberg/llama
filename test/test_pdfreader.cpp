#include <catch2/catch_test_macros.hpp>

#include "pdfreader.h"
#include "readseek_impl.h"
#include <iostream>


TEST_CASE("testExtractText") {
  auto testFile = std::fopen("test/data/small.pdf", "rb");
  std::shared_ptr<FILE> fp(testFile, std::fclose);
  ReadSeekFile rs(fp);
  PDFReader pr;
  pr.readTextFromPDF(rs);
  REQUIRE(std::string(pr.getExtractedText()) == "Dummy PDF file\n");
}