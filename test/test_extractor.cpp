
#include <catch2/catch_test_macros.hpp>

#include "extractor.h"
#include "readseek_impl.h"

#include <thread>
#include <vector>

TEST_CASE("testExtract") {
  auto file = std::fopen("test/data/test.pdf", "rb");
  std::shared_ptr<FILE> fileptr(file, std::fclose);

  ReadSeekFile rs(fileptr);
  auto extracted_text = Extractor::extract_pdf(rs);
  std::string expected_text = "Dummy PDF file\n";
  REQUIRE(extracted_text.get() == expected_text);
}