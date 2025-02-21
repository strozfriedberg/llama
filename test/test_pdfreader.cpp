#include <catch2/catch_test_macros.hpp>

#include "pdfreader.h"
#include "PDFDoc.h"
#include "TextOutputDev.h"
#include "gtypes.h"


// TEST_CASE("testExtractText") {
//   std::string testPath = "test/data/small.pdf";
//   GString *gStr = new GString(testPath.c_str());
//   PDFDoc doc = PDFDoc(gStr);
//   TextOutputControl textOutControl;
//   textOutControl.mode = textOutReadingOrder;
//   std::string* output = new std::string("");
//   TextOutputFunc func;
//   TextOutputDev *textOut = new TextOutputDev(func, output, &textOutControl);
//   REQUIRE(textOut->isOk());
//   doc.displayPages(textOut, 1, 0, 72, 72, 0, gFalse, gTrue, gFalse);
//   REQUIRE(*output == "");
//   delete output;
// }