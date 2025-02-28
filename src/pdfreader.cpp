#include <string>

#include "pdfreader.h"
#include "readseek_c_api.h"
#include "readseek_impl.h"

PDFReader::PDFReader() {
  this->Interface = { 
    .obj = nullptr,
    .read = readseek_read,
    .seek = readseek_seek,
    .tellg = readseek_tellg,
    .size = readseek_size,
  };
  ExtractedText = nullptr;
}

PDFReader::~PDFReader() {
  free_extracted_text(ExtractedText);
}

bool PDFReader::readTextFromPDF(ReadSeek& rs) {
  Interface.obj = &rs;
  ExtractedText = extract_text_from_pdf_readseek(Interface);
  return true;
}