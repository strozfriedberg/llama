#include <string>

#include "pdfreader.h"
#include "PDFDoc.h"
#include "TextOutputDev.h"

bool PDFReader::readText(std::string filePath) {
  GString *gStr = new GString(filePath.c_str());
  PDFDoc doc = PDFDoc(gStr);
  return doc.okToCopy();
}